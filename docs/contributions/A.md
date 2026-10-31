# A — структура та розбір C++ коду

Облік за знімком репозиторію `6ab06ce31638adc6bbf552932a728fd2145fc677` (гілка `main`, 2026-10-19). Власник модуля — A (`chillsmurfkkk`); це не означає автоматичного авторства кожного рядка. Початкові декларації A у E0 підтверджує авторство коміту `c8ed63c17be2dbaaa0dc2ccdeccc6c6106e1573f`; наступні алгоритми A підтверджуються історією файлів і комітів, зокрема `3fcc4109cbbcd4a7121d34bdfa8b81a7cfad3a17` фіксує автором A сам реєстр. У спільному коміті E0 автором є A, а комітером — `dan`; подальший portability guard у `source.cpp` належить B (`4fe0cf1`). Рішення про зарахування приймає викладач.

Наведені нижче стани означають наявність коду та/або перевірки, не прийняття лабораторної. Посилання ведуть до файлів від кореня `code-xray/docs/contributions/`.

## Робоче оформлення GUI — 2026-10-26

На основі покроково написаного A каркаса Codex за прямим запитом користувача
доробив тему, навігацію, векторні іконки й порожні стани «Структури» та додав
поведінкову перевірку GUI. Походження цього доповнення — AI-assisted; цей запис
не приписує його самостійному виконанню й не змінює лічильники ЛР №1.

Файли: `src/ui/shared/{theme,icons,navigation}.*`, `src/ui/code/structure_page.*`,
`src/app/main_window.cpp`, `tests/ui/design_tests.cpp` та відповідні цілі CMake.
Перевірки й рендери: [журнал оформлення](../gui-design-verification.md).
Підключення результатів парсера, чотири форми та облік обробників ЛР №2 ще
не завершені; поточний дизайн не зараховується замість цих вимог.

## GUI: приймання знімків і діапазони — 2026-10-26

A покроково написав за інструкціями Codex приймання `ParsedSnapshot`, дерево,
демонстраційну фікстуру, делегат вибору й відкриття коду. За наступним прямим
запитом Codex завершив позначення діапазонів, підтримку тем, відображення шляху,
різні порожні стани та інтеграційні перевірки. Це AI-assisted зміна; кількісні
підсумки ЛР №1 та приймання чотирьох форм ЛР №2 не змінюються.

Докази та контракт для координатора C: [GUI-знімки](../gui-snapshots.md).
Бекенд перевірено на реальному `collect()` → `parse()` у worker-потоці тесту;
вибір папки та запуск цих операцій із GUI до цього етапу не входять.

## GUI: відкриття папки — 2026-10-27

A покроково реалізував за інструкціями Codex вибір папки, запуск збору й
розбору в `QThread`, скасування та повторне використання відкритого документа.
За запитом A Codex додав тимчасовий параметр `--no-uia`, автоматичні перевірки
та документацію. Це AI-assisted робота; лічильники ЛР №1 не змінено. Повноцінний
координатор C і виправлення взаємодії Qt з Windows UI Automation залишаються
відкритими. Межі й перевірки описані в [GUI-знімках](../gui-snapshots.md).

## Підсумок

Цей розділ зберігає ID початкового реєстру; актуальність коду звірено за знімком SHA, наведеним вище.

| Категорія | Обліковано тут | Правило підрахунку |
|---|---:|---|
| Типи | 22 | Точний підрахунок власних типів, включених до цього реєстру; це не інвентар усіх оголошень модуля. Без enum, alias, бібліотечних і тестових типів. |
| Поля | 15 | Облікована вибірка фактично використовуваних полів; це не загальна кількість полів модуля. Успадковані поля враховано лише в базовому класі. |
| Нетривіальні реалізації | 37 | Окремі функції/методи з алгоритмом або перевірками; без конструкторів, getters, декларацій і тестових функцій. |
| Ієрархії | 2 | `SourceProvider` із двома нащадками та `CodeEntity` із трьома. |
| Випадки поліморфізму | 3 | Два динамічні Interface та один шаблонний обхід із двома різними клієнтами. Для `CodeEntity` наявний базовий виклик у тесті функції; демонстрацію всіх трьох нащадків ще слід доповнити. |

### Типи

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A-T01 | 1 | A (chillsmurfkkk) | Тип: Абстрактна сутність; ID, діапазон і дерево дітей | [types.hpp](../../src/code/types.hpp): `CodeEntity` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-T02 | 1 | A (chillsmurfkkk) | Тип: Корінь структури файла й утримання оригінальних байтів | [types.hpp](../../src/code/types.hpp): `FileEntity` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-T03 | 1 | A (chillsmurfkkk) | Тип: Функція, ключ зіставлення, тіло, статус і керувальне дерево | [types.hpp](../../src/code/types.hpp): `FunctionEntity` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declaratorShapes` |
| A-T04 | 1 | A (chillsmurfkkk) | Тип: Клас/структура з кваліфікованим ім'ям та вкладеними сутностями | [types.hpp](../../src/code/types.hpp): `TypeEntity` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-T05 | 1 | A (chillsmurfkkk) | Тип: Абстрактний поліморфний Interface збирання файлів | [api.hpp](../../src/code/api.hpp): `SourceProvider` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful` |
| A-T06 | 1 | A (chillsmurfkkk) | Тип: Обхід каталогу та передавання вибірки на читання | [api.hpp](../../src/code/api.hpp): `DirectoryProvider` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful` |
| A-T07 | 1 | A (chillsmurfkkk) | Тип: Читання явного списку з відбором і дедуплікацією | [api.hpp](../../src/code/api.hpp): `FileListProvider` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A-T08 | 1 | A (chillsmurfkkk) | Тип: Результат одного файла; дерево, індекс функцій і діагностика | [types.hpp](../../src/code/types.hpp): `ParsedUnit` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-T09 | 1 | A (chillsmurfkkk) | Тип: Причина помилки розбору та її діапазон | [types.hpp](../../src/code/types.hpp): `ParseDiagnostic` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `syntaxDiagnosticsHaveConcreteRanges` |
| A-T10 | 1 | A (chillsmurfkkk) | Тип: Напіввідкриті байтові межі й номери рядків від 1 | [types.hpp](../../src/code/types.hpp): `SourceRange` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-T11 | 1 | A (chillsmurfkkk) | Тип: Розширення, виключені каталоги та явні шляхи | [types.hpp](../../src/code/types.hpp): `FileSelection` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A-T12 | 1 | A (chillsmurfkkk) | Тип: Корінь локального джерела та правила вибірки | [types.hpp](../../src/code/types.hpp): `SourceRequest` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `invalidSelectedPathIsSkipped` |
| A-T13 | 1 | A (chillsmurfkkk) | Тип: Відносний шлях, незмінні байти та content ID | [types.hpp](../../src/code/types.hpp): `SourceFile` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `sourceBytesArePreserved` |
| A-T14 | 1 | A (chillsmurfkkk) | Тип: Зібране джерело з вибіркою та Coverage | [types.hpp](../../src/code/types.hpp): `SourceSnapshot` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `snapshotAndContentIdsBehaveCorrectly` |
| A-T15 | 1 | A (chillsmurfkkk) | Тип: Власне дерево керувальних конструкцій без TSNode | [types.hpp](../../src/code/types.hpp): `ControlNode` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `controlTreePreservesBranchLevels` |
| A-T16 | 1 | A (chillsmurfkkk) | Тип: Версії граматики та власної структури | [types.hpp](../../src/code/types.hpp): `ParseOptions` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `invalidInputs` |
| A-T17 | 1 | A (chillsmurfkkk) | Тип: Результати файлів з утриманням вихідного знімка | [types.hpp](../../src/code/types.hpp): `ParsedSnapshot` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `sourceLifetime` |
| A-T18 | 1 | A (chillsmurfkkk) | Тип: Текст пошуку та необов'язковий фільтр виду | [search.hpp](../../src/code/search.hpp): `SearchQuery` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions` |
| A-T19 | 1 | A (chillsmurfkkk) | Тип: Власний ID результату та ознака точного збігу | [search.hpp](../../src/code/search.hpp): `SearchMatch` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `searchResultsOwnTheirIds` |
| A-T20 | 1 | A (chillsmurfkkk) | Тип: Пошук через шаблонний обхід; приватні запит і результати | [search.hpp](../../src/code/search.hpp): `SearchVisitor` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions` |
| A-T21 | 1 | A (chillsmurfkkk) | Тип: ID сутності та конкретне пояснення суперечності | [validation.hpp](../../src/code/validation.hpp): `RangeIssue` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `rangesDetectByteParentViolations` |
| A-T22 | 1 | A (chillsmurfkkk) | Тип: Перевірка діапазонів; приватні розмір джерела й проблеми | [validation.hpp](../../src/code/validation.hpp): `RangeValidationVisitor` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `rangesContinueAfterInvalidEntities` |

### Поля

До вибірки не включено `FunctionEntity::parameterCount` і `qualifiers`: вони оголошені, але поточний парсер їх не заповнює. Замість них обліковано реально використовувані `identityKey` і `controlTree`. Просте оголошення поля не є окремою нетривіальною функцією.

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A-P01 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `CodeEntity::id` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `entityIdsAreUniqueAndRepeatable` |
| A-P02 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `CodeEntity::name` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declaratorShapes` |
| A-P03 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `CodeEntity::kind` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions` |
| A-P04 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `CodeEntity::range` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-P05 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `CodeEntity::children` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-P06 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FileEntity::relativePath` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-P07 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FileEntity::contentId` | Реалізовано | parseUnit копіює SourceFile::contentId; source:snapshotAndContentIdsBehaveCorrectly перевіряє ID джерел |
| A-P08 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FileEntity::languageId` | Реалізовано | parseUnit встановлює тип FileEntity із типовим languageId = cpp |
| A-P09 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FileEntity::sourceBytes` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `sourceLifetime` |
| A-P10 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FileEntity::parseState` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `syntaxErrorsPreserveValidFunctions` |
| A-P11 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FunctionEntity::signatureTokens` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declaratorTokensIgnoreFormatting` |
| A-P12 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FunctionEntity::bodyRange` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declarationsHaveNoApplicableBody` |
| A-P13 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FunctionEntity::ownerName` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `explicitlyQualifiedMethods` |
| A-P14 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FunctionEntity::identityKey` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `identityIgnoresBodyAndLineShifts` |
| A-P15 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../../src/code/types.hpp): `FunctionEntity::controlTree` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `controlTreeRecognizesLoopsAndLabels` |

### Нетривіальні функції та методи

ID A01–A25 збережено там, де є фактичний відповідник плану; A-F01–A-F20 позначають додаткові реалізації. Один символ враховано один раз. Рядок A17 враховує алгоритм `RangeValidationVisitor::operator()`; `validateEntityRanges` є клієнтом обходу та не збільшує кількість алгоритмів.

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A01 | 1 | A (chillsmurfkkk) | Обхід папки, відбір, Coverage, прогрес і скасування | [source.cpp](../../src/code/source.cpp): `DirectoryProvider::collect` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful` |
| A02 | 1 | A (chillsmurfkkk) | Перевірка явних шляхів, дедуплікація, читання й діагностика пропусків | [source.cpp](../../src/code/source.cpp): `FileListProvider::collect` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A03 | 1 | A (chillsmurfkkk) | Відхилення абсолютних шляхів і ..; лексична нормалізація | [source.cpp](../../src/code/source.cpp): `validateRelativePath` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `invalidSelectedPathIsSkipped` |
| A05 | 1 | A (chillsmurfkkk) | Поблокове читання; UTF-8, BOM, NUL, помилки й скасування | [source.cpp](../../src/code/source.cpp): `readFileBytes` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `sourceBytesArePreserved`; `invalidContentIsSkipped` |
| A07 | 1 | A (chillsmurfkkk) | RAII-дерево парсера; структура, діагностика та індекс одного файла | [parse.cpp](../../src/code/parse.cpp): `parseUnit` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A08 | 1 | A (chillsmurfkkk) | Обхід синтаксичних вузлів із namespace/type scope та вкладенням сутностей | [parse.cpp](../../src/code/parse.cpp): `walkStructure` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A09 | 1 | A (chillsmurfkkk) | Ім'я, scope, токени декларатора, тіло, статус і control tree | [parse.cpp](../../src/code/parse.cpp): `extractFunction` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declaratorShapes`; `declarationsHaveNoApplicableBody` |
| A10 | 1 | A (chillsmurfkkk) | Ім'я, діапазон і кваліфіковане ім'я class/struct/union | [parse.cpp](../../src/code/parse.cpp): `extractType` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A11 | 1 | A (chillsmurfkkk) | Розбір явної кваліфікації, глобального scope й спеціальних імен | [parse.cpp](../../src/code/parse.cpp): `extractQualifiedName` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `explicitlyQualifiedMethods` |
| A12 | 1 | A (chillsmurfkkk) | Листові токени без коментарів; відхилення ненадійного декларатора | [parse.cpp](../../src/code/parse.cpp): `normalizeDeclarator` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declaratorTokensIgnoreFormatting`; `literalContentsRemainSignificant` |
| A13 | 1 | A (chillsmurfkkk) | Побудова власного control tree; else-if/catch і пропуск внутрішніх тіл | [parse.cpp](../../src/code/parse.cpp): `appendControlNodes` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `controlTreePreservesBranchLevels`; `controlTreeExcludesInnerBodies` |
| A14 | 1 | A (chillsmurfkkk) | ERROR/MISSING у конкретні причини та діапазони; оновлення Coverage | [parse.cpp](../../src/code/parse.cpp): `collectParseDiagnostics` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `syntaxDiagnosticsHaveConcreteRanges`; `missingSyntaxTokenHasPointRange` |
| A15 | 1 | A (chillsmurfkkk) | Перетини діагностики з функцією та not_applicable без тіла | [parse.cpp](../../src/code/parse.cpp): `markAnalysisEligibility` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `eligibilityUsesConcreteDiagnosticReasons`; `eligibilityExcludesErrorsBetweenFunctions` |
| A17 | 1 | A (chillsmurfkkk) | Порядок, межі джерела й вкладення байтових та рядкових діапазонів | [validation.hpp](../../src/code/validation.hpp): `RangeValidationVisitor::operator()` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `rangesDetectByteParentViolations`; `rangesDetectLineParentViolations` |
| A19 | 1 | A (chillsmurfkkk) | Кодування шляху, кваліфікованого імені й токенів із довжинами UTF-8 | [parse.cpp](../../src/code/parse.cpp): `makeIdentityKey` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `identityUsesLengthPrefixedUtf8Fields`; `identityIgnoresBodyAndLineShifts` |
| A22 | 1 | A (chillsmurfkkk) | Запуск SearchVisitor і стабільне ранжування точних збігів перед частковими | [search.hpp](../../src/code/search.hpp): `searchEntities` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `searchRanksExactMatchesStably` |
| A25 | 1 | A (chillsmurfkkk) | Власний ітеративний обхід; батьки, порядок, skip_children і stop | [walk.hpp](../../src/code/walk.hpp): `walkEntities<Visitor>` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `walkPreservesOrderAndParents`; `walkSkipsOnlySelectedSubtree`; `walkStopsBeforePendingBranches` |
| A-F01 | 1 | A (chillsmurfkkk) | Розрізнення порожнього, відсутнього, нечитабельного кореня й не-каталогу | [source.cpp](../../src/code/source.cpp): `validateRoot` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful (коректний корінь)`; `негативні гілки validateRoot потребують окремої перевірки` |
| A-F02 | 1 | A (chillsmurfkkk) | Перевірка компонентів; symlink/junction, вкладений Git і звичайний файл | [source.cpp](../../src/code/source.cpp): `validateSourceFile` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `invalidSelectedPathIsSkipped`; `nestedGitTreesAreSkipped` |
| A-F03 | 1 | A (chillsmurfkkk) | Виявлення вкладених Git working trees та помилок читання .git | [source.cpp](../../src/code/source.cpp): `nestedGitExclusion` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `nestedGitTreesAreSkipped` |
| A-F04 | 1 | A (chillsmurfkkk) | Перевірка continuation bytes, мінімального code point і діапазонів Unicode | [source.cpp](../../src/code/source.cpp): `isValidUtf8` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `invalidContentIsSkipped` |
| A-F05 | 1 | A (chillsmurfkkk) | Детермінований обхід із причинами пропусків, прогресом і скасуванням | [source.cpp](../../src/code/source.cpp): `scanDirectory` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful`; `nestedGitTreesAreSkipped`; `cancellationNeverReturnsSnapshot` |
| A-F06 | 1 | A (chillsmurfkkk) | Відбір за каталогами й розширеннями з конкретною причиною виключення | [source.cpp](../../src/code/source.cpp): `filterExclusionReason` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A-F07 | 1 | A (chillsmurfkkk) | FNV-1a по оригінальних байтах, hex-кодування й перевірки скасування | [source.cpp](../../src/code/source.cpp): `makeContentId` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `snapshotAndContentIdsBehaveCorrectly` |
| A-F08 | 1 | A (chillsmurfkkk) | Ідентифікатор сесії та атомарний лічильник незалежних запусків | [source.cpp](../../src/code/source.cpp): `makeSnapshotId` | Реалізовано | [source_provider_test.cpp](../../tests/source_provider_test.cpp): `snapshotAndContentIdsBehaveCorrectly` |
| A-F09 | 1 | A (chillsmurfkkk) | RAII створення парсера й обробка несумісної граматики або збою створення | [parse.cpp](../../src/code/parse.cpp): `createParser` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `emptyInputs`; `invalidInputs (успішне створення)`; `гілки збою окремо не інжектовано` |
| A-F10 | 1 | A (chillsmurfkkk) | Перевірка байтового буфера/ліміту, розбір, RAII й скасування | [parse.cpp](../../src/code/parse.cpp): `parseTree` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `invalidInputs`; `cancellationFromProgress` |
| A-F11 | 1 | A (chillsmurfkkk) | Перевірка меж вузла перед виділенням фрагмента оригінальних байтів | [parse.cpp](../../src/code/parse.cpp): `nodeText` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `originalBytesAndRanges`; `invalidInputs` |
| A-F12 | 1 | A (chillsmurfkkk) | Перетворення координат у напіввідкриті байти й endLine останнього токена | [parse.cpp](../../src/code/parse.cpp): `nodeRange` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `originalBytesAndRanges`; `controlTreeKeepsOriginalRanges` |
| A-F13 | 1 | A (chillsmurfkkk) | Розрізнення else-if за контекстом та case/default за дочірніми токенами | [parse.cpp](../../src/code/parse.cpp): `controlKindFor(TSNode)` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `controlTreePreservesBranchLevels`; `controlTreeRecognizesLoopsAndLabels` |
| A-F14 | 1 | A (chillsmurfkkk) | Збирання значущих токенів і пробілів між словесними компонентами | [parse.cpp](../../src/code/parse.cpp): `normalizeName` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `formattedNamespacePreservesIdentity`; `formattedTemplateOwnerPreservesIdentity` |
| A-F15 | 1 | A (chillsmurfkkk) | Пошук імені всередині pointer/reference/parenthesized declarator | [parse.cpp](../../src/code/parse.cpp): `findDeclaratorName` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declaratorShapes` |
| A-F16 | 1 | A (chillsmurfkkk) | Відрізнення оголошення функції від pointer/member-pointer declarator | [parse.cpp](../../src/code/parse.cpp): `findDeclaredFunction` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `declarationsDistinguishFunctionsFromPointers`; `memberFunctionPointersAreNotDeclarations` |
| A-F17 | 1 | A (chillsmurfkkk) | Пошук умовної компіляції в предках функції | [parse.cpp](../../src/code/parse.cpp): `hasConditionalAncestor` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `conditionalAncestorsMarkAllBranches` |
| A-F18 | 1 | A (chillsmurfkkk) | Обхід тіла для виявлення умовної компіляції з підтримкою скасування | [parse.cpp](../../src/code/parse.cpp): `hasConditionalDescendant` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `conditionalBodyPreservesControlTree` |
| A-F19 | 1 | A (chillsmurfkkk) | Перевірка входів/версій, складання ParsedSnapshot, Coverage, прогрес і скасування | [parse.cpp](../../src/code/parse.cpp): `parse` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `inheritedCoverageAndMultipleFiles`; `progress`; `cancellationKeepsDiagnostics` |
| A-F20 | 1 | A (chillsmurfkkk) | Відбір за назвою/видом без пропуску вкладених функцій; запис exact/partial | [search.hpp](../../src/code/search.hpp): `SearchVisitor::operator()` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions`; `searchIsCaseSensitiveAndHandlesNoMatches` |

У число 37 не включено прості допоміжні `pathToUtf8`, `isExcludedDirectory`, `controlKindFor(std::string_view)`, `joinScope`, `makeEntityId`, фасад `collect`, обгортку `validateEntityRanges`, реалізації `describe`, getters visitor, defaulted `operator==`, конструктори й деструктори.

### Ієрархії та клієнтський поліморфізм

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A-I01 | 1 | A (chillsmurfkkk) | Ієрархія з 3 класів: SourceProvider → DirectoryProvider, FileListProvider | [api.hpp](../../src/code/api.hpp): `SourceProvider, DirectoryProvider, FileListProvider` | Реалізовано | Обидві реалізації collect у [source.cpp](../../src/code/source.cpp); сценарії папки та явного списку в [source_provider_test.cpp](../../tests/source_provider_test.cpp). |
| A-I02 | 1 | A (chillsmurfkkk) | Ієрархія з 4 класів: CodeEntity → FileEntity, FunctionEntity, TypeEntity | [types.hpp](../../src/code/types.hpp): `CodeEntity, FileEntity, FunctionEntity, TypeEntity` | Реалізовано | Абстрактний describe та overrides; [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex`. |
| A-DYN01 | 1 | A (chillsmurfkkk) | Динамічний вибір збирання через SourceProvider& | [source.cpp](../../src/code/source.cpp): `collect` | Реалізовано | Фасад викликає `provider.collect` через базове посилання для папки й списку; `emptyDirectoryIsSuccessful` та `explicitSelectionDeduplicatesAndFilters`. |
| A-DYN02 | 1 | A (chillsmurfkkk) | Віртуальний опис через CodeEntity& | [types.hpp](../../src/code/types.hpp): `CodeEntity::describe; overrides у нащадках` | Реалізовано | [parser_tests.cpp](../../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` викликає describe через `const CodeEntity&` для FunctionEntity; показ FileEntity/TypeEntity через ту саму базу — наступна демонстрація. |
| A-STATIC01 | 1 | A (chillsmurfkkk) | Статичний поліморфізм: walkEntities<SearchVisitor> та walkEntities<RangeValidationVisitor> | [walk.hpp](../../src/code/walk.hpp): `walkEntities<Visitor>` | Перевірено | Клієнти [search.hpp](../../src/code/search.hpp): `searchEntities` і [validation.hpp](../../src/code/validation.hpp): `validateEntityRanges`; `searchKindFilterStillVisitsNestedFunctions`, `rangesAcceptParsedEntities`. |

### Межі обліку та перевірка

- A03 відповідає лексичній нормалізації шляхів; перевірка реальних компонентів і symlink/junction окремо облікована в A-F02. Видимість прихованих файлів не налаштовується, тому повний початковий кандидат A04 не позначено виконаним.
- A09 не заявляє підрахунок параметрів і заповнення qualifiers. Умовна компіляція позначається `syntactic_only` у `extractFunction` через A-F17/A-F18; A15 відповідає пересіченню діагностики й функції.
- A17 перевіряє порядок, межі та вкладення координат; не доводить відповідність кожного номера рядка байтовому зміщенню. Окремого індексу рядків A06 немає.
- A16 не внесено як окремий алгоритм: ID призначаються лічильником під час parse/walkStructure, а дубльовані ключі зберігаються для споживача. A18, A20, A21, A23 та A24 окремо не реалізовані й не входять у квоту.
- У SearchVisitor та RangeValidationVisitor стан приватний. Сутності та знімки мають публічні поля й передаються як `shared_ptr<const ...>`; це фактичний поточний дизайн, а не твердження про виконання початкового плану закритих полів сутностей. Інкапсуляцію сутностей ще слід окремо звірити перед захистом.
- Тестові функції не входять у число 37. Для getters, конструкторів, бібліотечного та згенерованого коду немає окремого зарахування. Цей розділ не заявляє виконання GUI ЛР №2 або ретроспективи ЛР №3.

Історичний запуск від 2026-10-13: `cmake --build --preset windows-x64 --target parser_tests`, потім `ctest --preset windows-x64 -R "^parser$" --output-on-failure`; пройшли всі 63 сценарії, включно з обходом, пошуком і перевіркою діапазонів. Поточний спільний результат Windows build/CTest фіксується в [журналі перевірки ЛР1](../lab1/lab1-verification.md); історичний запуск не засвідчує перевірку `source_provider_test`.

## ЛР №2: пошук і фільтри структури

Оновлено 2026-10-28. На сторінці `StructurePage` реалізовані поле пошуку,
вибір виду сутності, лічильник прямих збігів, збереження батьківських вузлів
і стан «Збігів немає» зі скиданням. `applyFilters` і рекурсивний `filterItem`
працюють із готовим деревом без повторного розбору; прихований вибір очищає код.
Перевірки в `tests/ui/snapshot_tests.cpp` використовують справжній результат
парсера та охоплюють вкладені методи, поєднання критеріїв, збереження знімка,
зміну теми, скидання й різні порожні стани. Це доповнення до GUI ЛР №2;
історичні числа ЛР №1 вище не змінюються й не означають завершення чотирьох форм.

Спільне оформлення доповнено `PopupStyle` у `src/ui/shared/popup_style.hpp`:
округлі маски меню й підказок, узгоджені з QSS, без проміжних анімаційних
вікон. `gui_popup` перевіряє обидві теми, кути, resize та клавіатурну взаємодію;
повний CTest від 2026-10-28 — 20/20. Це спільний компонент, а не додаткова форма A.

## ЛР №2: номери рядків у перегляді коду

Оновлено 2026-10-28. Додано `CodeView`, нащадок `QPlainTextEdit`, і допоміжний
віджет `LineNumberArea` у [code_view.hpp](../../src/ui/code/code_view.hpp) та
[code_view.cpp](../../src/ui/code/code_view.cpp). Переглядач зберігає індекс
LF-рядків у позиціях UTF-16, малює номери окремо від тексту й оновлює ширину
колонки за кількістю цифр і шрифтом. `StructurePage` використовує цей самий
індекс для переходів та позначення діапазонів, завантажує й очищає текст через
`setSourceText()`; код зі знімка, робоче виділення та копіювання зберігаються.

Windows-збірка застосунку й тестових цілей успішна; `gui_snapshot`, `gui_design`,
`gui_startup` та `gui_demo_startup` пройшли — 4/4. Рендери переглянуто в обох
темах; розробник вручну перевірив прокручування, трицифрові номери, звуження
колонки та копіювання без номерів. Деталі й межі перевірки — у
[контракті GUI-знімків](../gui-snapshots.md). Ця зміна доповнює сторінку
«Структура», не змінює історичні числа ЛР №1 і не додає окремої форми.

## ЛР №2: підсвічування синтаксису

Оновлено 2026-10-28. Додано [CppSyntaxHighlighter](../../src/ui/code/cpp_syntax_highlighter.hpp)
із послідовним розпізнаванням ключових слів C++20, числових токенів, літералів
із префіксами, raw strings, коментарів і директив препроцесора. Стан блоків
зберігає роздільник raw string і продовження літералів, коментарів та директив;
індекс LF-рядків дозволяє відрізняти межі вихідних рядків від Unicode-блоків Qt.
`CodeView` підключає підсвічувач до документа й оновлює його за спільною
палітрою застосунку. Кольори `SyntaxColors` централізовані в `ui/shared/theme`;
позначення сутності й виділення користувача зберігаються.

Додано [gui_syntax](../../tests/ui/syntax_tests.cpp), уточнено перевірку змін
тексту й кольору теми в `gui_snapshot`. Windows-збірка успішна, повний CTest
пройшов — 21/21; рендери обох тем переглянуто. Поведінку й лексичні обмеження
описано в [GUI-знімках](../gui-snapshots.md). Зміна доповнює наявний перегляд
коду й не змінює історичні числа ЛР №1.

## ЛР №2: сторінка «Джерело»

Оновлено 2026-10-31. Завершено A-UI1: власний `SourcePage` у спільному вікні,
вибір папки або окремих файлів, запуск, прогрес, скасування та повторний запуск.
Початкову сторінку й підключення написано A під час покрокової роботи;
завершення списку файлів, прогресу, станів і перевірок виконано з допомогою Codex.

| ID | Реалізація | Перевірка |
|---|---|---|
| A-UI1 | [source_page.hpp](../../src/ui/code/source_page.hpp), [source_page.cpp](../../src/ui/code/source_page.cpp), підключення в [MainWindow](../../src/app/main_window.cpp). | CTest `gui_source`, `gui_design`. |
| A-UI1-C01–C05 | `sourceDirectory`, `sourceBrowseDirectory`, `sourceBrowseFiles`, `sourceScan`, `sourceCancel` — п'ять запланованих контролів. | Введення кореня, папка або явний список, запуск, блокування й скасування. Нативні діалоги потребують ручної перевірки. |
| A-H01 | `SourcePage::sourceChanged`, `addFiles`: скидання відносних шляхів при зміні кореня, унікальні файли всередині нього та позначення потреби нового сканування. | Unicode-шлях, дублікати, зовнішні файли, видалення, повне скидання й зміна кореня. |
| A-H02 | `SourcePage::requestScan`, `MainWindow::loadSource`: перевірка кореня, копія `SourceRequest`, фоновий запуск і доставка прогресу до GUI. | Справжній collect/parse для списку й папки, порожній/частковий результат, помилка worker та повторний запуск. |
| A-H03 | `MainWindow::cancelSource`, `SourcePage::setCancelling`: stop token, очікування завершення, відхилення скасованого результату й відновлення контролів. | Скасування до GUI-доставки результату, незмінність попереднього дерева та закриття активного вікна. |

Додатково є список файлів, видалення вибраних рядків, повернення до всієї папки,
індикатор прогресу, текст результату та перехід до «Структури». Вони не
зараховані тут повторно як окремі форми. Прогрес і скасування використовують
наявні контракти ядра; загальний координатор C не оголошено завершеним.
Числа ЛР №1 залишаються незмінними. Подальший стан сторінок відбору та
діагностики описано нижче.

Поведінка, команди, рендери та межі перевірки — у [gui-source.md](../gui-source.md).

## ЛР №2: чотири сторінки коду

Звірено з кодом гілки `a/gui-source-page` на коміті `44f4937` (2026-10-31).
A писав початковий код сторінок за покроковими поясненнями Codex; за запитом A
Codex доповнив оформлення й тести відбору та завершив діагностику, навігацію,
перевірки й документацію. Це спільна робота з допомогою AI: наведений перелік
показує фактичні елементи та походження, а не підтверджує їх самостійне
виконання чи автоматичне зарахування на захисті.

| Форма | Фактичні елементи керування для перевірки | Код і межі |
|---|---|---|
| A-UI1 «Джерело» | `sourceDirectory`, `sourceBrowseDirectory`, `sourceBrowseFiles`, `sourceScan`, `sourceCancel`, `sourceFiles`, `sourceRemoveFiles`, `sourceClearFiles` — 8. | [source_page.cpp](../../src/ui/code/source_page.cpp); список шляхів, запуск і скасування. |
| A-UI2 «Відбір файлів» | `extensionsInput`, `excludedDirectories`, `exclusionInput`, `addExclusion`, `removeExclusion` — 5. | [file_selection_page.cpp](../../src/ui/code/file_selection_page.cpp); прапорця прихованих файлів немає в контракті та GUI. |
| A-UI3 «Структура» | `entitySearch`, `entityKindFilter`, `structureTree`, `sourceCode`, `resetEntityFilters` — 5. | [structure_page.cpp](../../src/ui/code/structure_page.cpp); окремої запланованої кнопки розгортання дерева немає, є скидання фільтрів. |
| A-UI4 «Діагностика» | `diagnosticsTable`, `diagnosticsKindFilter`, `diagnosticsPathFilter`, `diagnosticsReparse`, `diagnosticsOpenCode` — 5. | [diagnostics_page.cpp](../../src/ui/code/diagnostics_page.cpp); список розрізняє збір і розбір, бо рівнів критичності в ядрі немає. |

Разом наведено 23 різні контроли-кандидати та чотири сторінки; остаточне
зарахування контролів і авторства визначає викладач. Контейнери включають
`QFrame` робочих панелей, `QScrollArea` форми відбору та `QSplitter` структури.
Спільну навігацію, заголовки й використання чужих сторінок до цього переліку
не включено.

| ID із плану | Фактична поведінка та відмінності |
|---|---|
| A-H01–A-H03 | Зміна джерела, запуск та скасування описані в попередньому розділі; використовується тимчасовий запуск у `MainWindow`. |
| A-H04 | `FileSelectionPage::applyTo` перевіряє розширення й переносить їх у запит при скануванні; перевірка не запускається на кожен символ. |
| A-H05–A-H06 | Обробники кнопок відбору додають унікальне ім'я каталогу та видаляють вибрані; нові правила впливають на наступне сканування. |
| A-H07–A-H09 | Пошук і вид сутності оновлюють дерево через `StructurePage::applyFilters`; вибір показує код знімка через `onEntitySelected`. |
| A-H10 | `DiagnosticsPage::applyFilters` поєднує шлях і тип повідомлення, показує кількість збігів та стан без збігів. |
| A-H11 | Кнопка повторного сканування передає запит до `SourcePage::requestScan` і використовує поточне джерело та правила; окремого рушія повторного розбору немає. |
| A-H12 | `DiagnosticsPage::openSelected` і `StructurePage::openLocation` відкривають доступний файл знімка та рядок; для непрочитаного файла дія недоступна. |

Це відповідність поведінки плановим ID, а не твердження, що всі 12 є окремими
самостійно написаними обробниками: частина використовує спільні методи й
сигнали. Кількість змістовних обробників для захисту потребує особистої звірки.
На `44f4937` Windows-збірка цілей GUI та `gui_design`, `gui_source`,
`gui_snapshot` пройшли; `gui_source` повторно пройшов після доповнення
перевірки прокручування. Повний CTest на цьому коміті й macOS не перевірялися.
Підключення остаточного координатора C та особиста демонстрація залишаються.
