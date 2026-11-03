#include "code/api.hpp"
#include "history/api.hpp"
#include "ui/code/structure_page.hpp"

#include <QApplication>
#include <QFile>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <git2.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename T, void (*Free)(T*)>
using Handle = std::unique_ptr<T, decltype(Free)>;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    git_libgit2_init();
    int result = 0;
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "Cannot create temporary repository");
        git_repository* repoPtr = nullptr;
        require(git_repository_init(&repoPtr, directory.path().toUtf8().constData(), 0) == 0,
                "Cannot initialize repository");
        Handle<git_repository, git_repository_free> repo(repoPtr, git_repository_free);

        const std::string original = "// committed\nint from_commit() {\n    return 42;\n}\n// end\n";
        git_oid blobOid{}, treeOid{}, commitOid{};
        require(git_blob_create_from_buffer(&blobOid, repo.get(), original.data(), original.size()) == 0,
                "Cannot create blob");
        git_treebuilder* builderPtr = nullptr;
        require(git_treebuilder_new(&builderPtr, repo.get(), nullptr) == 0, "Cannot create tree builder");
        Handle<git_treebuilder, git_treebuilder_free> builder(builderPtr, git_treebuilder_free);
        require(git_treebuilder_insert(nullptr, builder.get(), "history.cpp", &blobOid, GIT_FILEMODE_BLOB) == 0 &&
                git_treebuilder_write(&treeOid, builder.get()) == 0, "Cannot create tree");
        git_tree* treePtr = nullptr;
        require(git_tree_lookup(&treePtr, repo.get(), &treeOid) == 0, "Cannot look up tree");
        Handle<git_tree, git_tree_free> tree(treePtr, git_tree_free);
        git_signature* signaturePtr = nullptr;
        require(git_signature_now(&signaturePtr, "Test", "test@example.com") == 0, "Cannot create signature");
        Handle<git_signature, git_signature_free> signature(signaturePtr, git_signature_free);
        require(git_commit_create_v(&commitOid, repo.get(), "HEAD", signature.get(), signature.get(),
                                    nullptr, "test", tree.get(), 0) == 0, "Cannot create commit");
        char oid[GIT_OID_SHA1_HEXSIZE + 1]{};
        git_oid_tostr(oid, sizeof(oid), &commitOid);

        QFile workingFile(directory.path() + "/history.cpp");
        require(workingFile.open(QIODevice::WriteOnly), "Cannot create working file");
        require(workingFile.write("int changed_on_disk() { return 0; }\n") > 0, "Cannot write working file");
        workingFile.close();

        const auto path = std::filesystem::path(directory.path().toStdWString());
        const xray::JobContext context;
        auto loaded = xray::history::loadSnapshot({path}, oid, {}, context);
        require(std::holds_alternative<xray::code::SourceSnapshot>(loaded), "Cannot load commit snapshot");
        auto source = std::make_shared<const xray::code::SourceSnapshot>(
            std::get<xray::code::SourceSnapshot>(std::move(loaded)));
        require(source->kind == xray::code::SourceKind::git && source->commitOid == oid,
                "Snapshot must identify the selected commit");
        auto parsed = xray::code::parse(source, {}, context);
        require(std::holds_alternative<xray::code::ParsedSnapshot>(parsed), "Cannot parse commit snapshot");

        xray::ui::StructurePage page;
        page.setSnapshot(std::make_shared<const xray::code::ParsedSnapshot>(
            std::get<xray::code::ParsedSnapshot>(std::move(parsed))));
        auto* widget = page.findChild<QTreeWidget*>("structureTree");
        auto* editor = page.findChild<QPlainTextEdit*>("sourceCode");
        require(widget && editor && widget->topLevelItemCount() == 1, "Missing snapshot widgets or file");
        auto* file = widget->topLevelItem(0);
        require(file->text(0) == "history.cpp" && file->childCount() == 1, "Missing committed function");
        require(file->child(0)->text(0) == "from_commit", "Wrong function in snapshot");
        widget->setCurrentItem(file->child(0));
        app.processEvents();
        const QString expected = QString::fromStdString(original);
        require(editor->toPlainText() == expected, "Viewer must show commit bytes, not the working file");
        const auto selections = editor->extraSelections();
        require(!selections.isEmpty() &&
                selections.front().cursor.selectionStart() == expected.indexOf("int from_commit") &&
                selections.front().cursor.selectionEnd() == expected.indexOf("}\n") + 1,
                "Viewer must highlight the committed function range");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    git_libgit2_shutdown();
    return result;
}
