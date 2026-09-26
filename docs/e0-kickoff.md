# E0: спільний старт Code Xray

Платформа команди — **Windows x64**. Погоджений стек: **C++20, Qt 6 Widgets,
CMake, Tree-sitter із C++ граматикою, libgit2, nlohmann/json та inja**.
E0 додає порожній застосунок, компільовані контракти й контрольні дані.
Реалізація аналізу, Git-історії, порівняння та сторінок A–D починається наступними PR.

## Збірка

Потрібні Visual Studio 2022 із **Desktop development with C++**, MSVC v143,
Windows SDK, CMake 3.25+ та Git. Установлення сторонніх бібліотек — через vcpkg
manifest mode; окрема інсталяція Qt/MinGW не потрібна. Весь застосунок збирається
одним MSVC x64 toolchain, бібліотеки — для `x64-windows`.

Якщо vcpkg ще немає, виконай у PowerShell, замінивши шлях на власний:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:/dev/vcpkg
git -C C:/dev/vcpkg checkout 776a31fc8d3659de2c9cca4364062be5bbdf3857
& C:/dev/vcpkg/bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = 'C:/dev/vcpkg'
```

Для вже встановленого vcpkg достатньо задати `VCPKG_ROOT`; версії пакетів визначає
`builtin-baseline` у [vcpkg.json](../vcpkg.json). Перша збірка завантажує та компілює
Qt і решту залежностей, тому потребує мережі, вільного диска й часу.
Після цього з кореня репозиторію:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --preset windows-x64
& ./build/windows-x64/Debug/code-xray.exe
```

`windeployqt` копіює потрібні Qt DLL і platform plugin поруч із застосунком.
Це запуск на машині розробника з MSVC, ще не готовий інсталятор продукту.
Локальні шляхи зберігай у змінних середовища або ігнорованому `CMakeUserPresets.json`.

| Залежність | Зафіксована версія |
|---|---|
| vcpkg baseline | `776a31fc8d3659de2c9cca4364062be5bbdf3857` |
| Qt / qtbase | `6.11.1` з Widgets і windeployqt |
| Tree-sitter | `0.26.9` |
| tree-sitter-cpp | `v0.23.4`, commit `f41e1a044c8a84ea9fa8577fdd2eab92ec96de02` |
| libgit2 | `1.9.4#1`, pcre2; без SSH/HTTPS для локального сценарію |
| nlohmann/json | `3.12.0#2` |
| inja | `3.5.0#1` |

`#N` — ревізія пакування vcpkg. Транзитивні залежності також визначає baseline;
граматика збирається з конкретного Git commit у `cmake/Dependencies.cmake`.
Сумісність runtime/grammar перевіряється викликом `ts_parser_set_language` у тесті.
Механізм фіксації версій описаний в [офіційній документації vcpkg](https://learn.microsoft.com/en-us/vcpkg/consume/lock-package-versions).

Для роботи над контрактами без встановлення бібліотек є окремий режим:

```powershell
cmake --preset contracts-only
cmake --build --preset contracts-only
ctest --preset contracts-only
```

Він перевіряє лише C++ типи й контрольні дані; успіх цього режиму не підтверджує запуск Qt.

## Модулі та власники

| Власник | Каталоги та контракти |
|---|---|
| A — chillsmurfkkk | `src/code/`, `src/ui/code/`: SourceSnapshot, SourceFile, FileSelection, ParsedSnapshot, ParsedUnit, CodeEntity / FileEntity / FunctionEntity / TypeEntity, ParseDiagnostic, ControlNode, SourceProvider. Головне вікно — `src/app/main.cpp`. |
| B — Kolya0685 | `src/analysis/`, `src/ui/analysis/`: AnalysisProfile, AnalysisSnapshot, MetricResult, Finding; наступні PR реалізують Metric / LengthMetric / NestingMetric / BranchCountMetric та Rule / LongFunctionRule / DeepNestingRule / ManyBranchesRule. |
| C — kostukoleksandr | `src/common/`, `src/history/`, `src/ui/history/` і майбутній координатор `src/app/`: Result, Error, Cancelled, Coverage, JobContext, CommitRecord, ChangeRecord, HistoryResult. |
| D — Knuuniacc | `src/review/`, `src/ui/review/`: ComparisonReport, ReviewItem, TrendReport, параметри та типи експорту; наступні PR реалізують ComparisonRule та ReportExporter із погодженими похідними класами. |

Namespaces: `xray` для common; `xray::code`, `xray::analysis`, `xray::history`,
`xray::review` для модулів. Заголовки підключаються від `src`, наприклад `code/api.hpp`.
Кожен власник додає свої тести в `tests/<module>/`; спільні приклади вже є в `tests/fixtures.hpp`.
Каталоги GUI створюються разом із першими сторінками, без порожніх заглушок.

Залежності типів і CMake targets:

```text
common ← code ← analysis ← review
            ↖ history
app → code, analysis, history, review
```

`history` залежить від моделі `code`, але не запускає парсер; `review` споживає
результати `analysis`, але не запускає метрики чи Git. `app` визначає послідовність
`SourceSnapshot → ParsedSnapshot → AnalysisSnapshot → ComparisonReport`.
Жоден модуль не підключає `app`; сторонні бібліотеки надалі лінкуються PRIVATE
до реалізацій відповідних модулів, а не поширюються через публічні типи.

`struct` у публічних заголовках — типізовані дані, які модулі передають один одному.
ООП живе в класах із поведінкою: A реалізує `SourceProvider`/`CodeEntity` і похідні,
B — `Metric`/`Rule`, C — `HistoryQuery`/`RevisionSelection`, D — `ComparisonRule`/
`ReportExporter`. E0 оголошує перші дві межі A, але не реалізує алгоритми за A–D.
Для лабораторних зараховується фактичний код класів і виклики через базовий клас,
а не сам факт, що заголовок містить `struct` чи `class`.

## Публічні операції

Точні сигнатури — в `src/{code,analysis,history,review}/api.hpp`.
Це синхронні операції для робочого потоку; вони повертають `Result<T>`.
Усі вісім отримують `const JobContext&`, включно з порівнянням, трендом та експортом.

| Модуль | Операції |
|---|---|
| code | `collect(SourceRequest, JobContext)`, `parse(shared_ptr<const SourceSnapshot>, ParseOptions, JobContext)` |
| analysis | `analyze(shared_ptr<const ParsedSnapshot>, AnalysisProfile, JobContext)` |
| history | `query(RepositorySpec, HistoryRequest, JobContext)`, `loadSnapshot(RepositorySpec, Oid, FileSelection, JobContext)` |
| review | `compare(base, target, ComparisonOptions, JobContext)`, `buildTrend(OrderedAnalyses, TrendOptions, JobContext)`, `exportReport(ComparisonReport, ExportOptions, JobContext)` |

`base` і `target` — `shared_ptr<const AnalysisSnapshot>`. Вхідні вказівники повинні
бути ненульовими; реалізація повертає `invalid_input` при порушенні цієї умови.
Операції лише оголошені в E0: вони не повертають фіктивний успіх і ще не викликаються GUI.
Стратегії на кшталт `HistoryQuery::execute` та `ReportExporter::write` залишаються
внутрішніми точками поліморфізму за цими фасадами; це не альтернативні API для `app`.

## Правила контрактів

1. **Незмінність і життя даних.** A/C формують SourceSnapshot, після завершення
   передають його як `shared_ptr<const SourceSnapshot>` і більше не змінюють жодних
   об'єктів через залишені mutable aliases. Байти — `shared_ptr<const string>`.
   ParsedSnapshot утримує джерело, AnalysisSnapshot — ParsedSnapshot, а звіт — обидва
   аналізи. GUI відкриває ці байти, а не поточний файл з диска. Індекс `ParsedUnit::functions`
   посилається на ті самі незмінні об'єкти, що й дерево `root`.
2. **Шляхи й координати.** ОС-шляхи — `std::filesystem::path`; відносні шляхи в
   знімку — UTF-8 із `/`, без абсолютного префікса та `..`. Регістр зберігається,
   включно з Git-шляхами на Windows. Діапазони байтів `[startByte,endByte)` посилаються
   на оригінальні UTF-8 байти з BOM/CRLF; `startLine/endLine` — 1-базовані включні
   рядки першого/останнього токена. Парсер не нормалізує переноси без відновлення координат.
3. **Повнота й невідомість.** `Result<T>` — `variant<T, Error, Cancelled>`. Успішний
   порожній T, помилка та скасування різні; Cancelled містить лише діагностики.
   Частковий успіх має `coverage.completeness = Completeness::partial`, кількість пропусків і причини; статус
   успадковується наступними етапами. Фільтр виключає файл навмисно, помилка читання
   вибраного файла робить результат частковим. Ліміт історії позначається `hasMore`.
   Недоступне значення — `nullopt`, не 0; діагностики зберігають шлях і контекст.
4. **Структура для B.** A віддає тіло функції, її повний діапазон, validity та дерево
   ControlNode. `else_if` розташовується на рівні відповідного `if`, `catch_clause` —
   на рівні `try_statement`; їхні тіла залишаються дітьми. Лямбди й локальні типи
   позначаються явно, щоб B пропускав їхні піддерева для вкладеності/розгалужень.
   A не обчислює метрики. Помилка, що перетинає функцію, дає `unavailable`;
   оголошення без тіла — `not_applicable`; умовна компіляція — `syntactic_only`.
5. **Метрики й профіль.** Усі три метрики рахуються завжди; вимикати можна правила.
   Довжина — `endLine-startLine+1`. Вкладеність рахує if, цикли, switch, try/catch;
   звичайні блоки не додають рівень. Розгалуження рахують if/else-if, цикли, case,
   catch, `?:`; не рахують default, else, switch, try, `&&`, `||`.
   Пороги default-v1: строго `>60`, `>3`, `>10`. Значення є лише для `valid` і
   `syntactic_only`; в останньому випадку попередження переходить у Finding.
6. **Сумісність.** B формує непорожній стабільний `analysisFingerprint` з версії
   схеми метрик/правил, ParseOptions, канонічних FileSelection та змісту профілю
   (schemaVersion, пороги, увімкнені правила). Відбиток не включає OID, байти,
   source ID, часові мітки, абсолютний корінь, назву/id профілю чи фактичний список
   знайдених файлів: вони можуть змінюватися між версіями. `std::hash` не є
   переносимим відбитком. Різні або порожні відбитки → `incompatible_analysis`.
7. **Точне зіставлення.** A створює ключ із відносного шляху, кваліфікованого імені
   й токенів декларатора без коментарів/форматування; кожне поле/токен кодується
   як `<кількість UTF-8 байтів>:<значення>`. Рядки не входять у ключ. EntityId унікальний
   лише в одному знімку й не замінює ключ. D не відновлює ключ і не вгадує перейменування.
   Дублі → `unmatched`; при неповному вході відсутність ключа не доводить додавання
   чи видалення: консервативно `unmatched`/`unknown`, поки повноту відповідного файла
   не доведено. Видалення коду — `removed_with_code`, не `resolved`.
   Різниця метрики — `after - before` зі знаком; якщо значення недоступне, delta — `nullopt`.
8. **Історія й тренд.** C читає Git blobs без checkout, використовує конкретні OID;
   root має порожнє дерево зліва, merge — обраного батька (індекс від 0).
   Обхід історії залишається first-parent незалежно від вибраного батька для diff.
   Координатор перевіряє належність усіх точок одному first-parent ланцюжку,
   включно з пропущеними між ними комітами, та формує OrderedAnalyses від старого до
   нового. D не перевіряє Git через libgit2; відсутня точка → пропуск, не нуль.
9. **Фонові операції.** JobContext має stop_token і callback прогресу з jobId;
   callback викликається в робочому потоці. C доставляє повідомлення в GUI через
   queued Qt connection і відкидає застарілий jobId. Перевірка скасування — між
   файлами, функціями, комітами та етапами; перед записом фінального звіту теж.
   У v1 працює одна задача за раз; профіль/налаштування копіюються на старті.
10. **Експорт і межі.** Шлях запису обирає користувач; перезапис підтверджується GUI.
    Тимчасовий файл замінює попередній лише після успіху. HTML екранує код, шляхи,
    повідомлення; inja не вважається автоматичним захистом. TSNode, libgit2-вказівники,
    QWidget та JSON не переходять між модулями замість типізованих результатів.

## Перевірка та вихід з E0

`contract_tests` перевіряє життя джерел, координати контрольної функції, стани
помилки/часткового успіху/скасування та прогрес із jobId. Окремо компілюється кожен
публічний заголовок. `dependency_smoke` перевіряє граматику, libgit2, JSON та inja;
`gui_startup` запускає вікно й завершує цикл подій. Очікувані дані —
[testdata/expected/e0.md](../testdata/expected/e0.md).

Ці перевірки не зараховують реалізацію A–D чи лабораторні. Для завершення командного
E0 кожен із чотирьох запускає повний preset на своїй машині, інший учасник переглядає
PR, а команда зливає його в main. Стан локальної перевірки записується в PR/коміті;
чужі машини та рев'ю не позначаються перевіреними наперед.

Перші PR після E0: A розбирає контрольні функції; B реалізує LengthMetric і правило
LongFunctionRule на фікстурі; C читає два реальні коміти без checkout; D реалізує
точне зіставлення та різницю довжини на готових AnalysisSnapshot. Координатор C
після цього збирає перший наскрізний сценарій.
