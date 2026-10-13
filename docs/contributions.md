# Реєстр фактичного внеску

| ID плану | ЛР | Автор | Елемент/тип | Файл і символ у коді | Стан | Перевірка/демонстрація |
|---|---|---|---|---|---|---|
| D01 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::compare` | Реалізовано | `tests/review/compare_tests.cpp::test_compare_*` |
| D02 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::validateCompatibility` | Реалізовано | `tests/review/compare_tests.cpp::test_validateCompatibility_*` |
| D03 | 1 | D (Ковальчук) | Функція (шаблон) | `src/review/matching.hpp::xray::review::indexEntities` | Реалізовано | `tests/review/compare_tests.cpp::test_indexEntities` |
| D04 | 1 | D (Ковальчук) | Функція (шаблон) | `src/review/matching.hpp::xray::review::matchEntities` | Реалізовано | `tests/review/review_tests.cpp` |
| D05 | 1 | D (Ковальчук) | Функція | `src/review/matching.hpp::xray::review::classifyUnmatched` | Реалізовано | `tests/review/compare_tests.cpp::test_classifyUnmatched` |
| D06 | 1 | D (Ковальчук) | Функція (шаблон) | `src/review/matching.hpp::xray::review::joinByKey` | Реалізовано | `tests/review/review_tests.cpp` |
| D07 | 1 | D (Ковальчук) | Функція (шаблон) | `src/review/matching.hpp::xray::review::compareMetricValues` | Реалізовано | `tests/review/compare_tests.cpp::test_compareMetricValues` |
| D08 | 1 | D (Ковальчук) | Метод класу | `src/review/rules.hpp::xray::review::MetricGrowthRule::evaluate` | Реалізовано | `tests/review/review_tests.cpp` |
| D09 | 1 | D (Ковальчук) | Метод класу | `src/review/rules.hpp::xray::review::NewFindingRule::evaluate` | Реалізовано | `tests/review/review_tests.cpp` |
| D10 | 1 | D (Ковальчук) | Метод класу | `src/review/rules.hpp::xray::review::ResolvedFindingRule::evaluate` | Реалізовано | `tests/review/review_tests.cpp` |
| D11 | 1 | D (Ковальчук) | Функція | `src/review/matching.hpp::xray::review::matchFindings` | Реалізовано | `tests/review/review_tests.cpp` |
| D12 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::summarizeComparison` | Реалізовано | `tests/review/compare_tests.cpp::test_summarize` |
| D13 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::groupReviewItems` | Реалізовано | `tests/review/compare_tests.cpp::test_group_sorts_by_path_then_kind` |
| D14 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::filterReviewItems` | Реалізовано | `tests/review/compare_tests.cpp::test_filter_*` |
| D15 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::rankReviewItems` | Реалізовано | `tests/review/compare_tests.cpp::test_rank_by_metric_delta` |
| D16 | 1 | D (Ковальчук) | Функція | `src/review/trend.cpp::xray::review::buildTrend` | Реалізовано | `tests/review/trend_tests.cpp::test_buildTrend_*` |
| D17 | 1 | D (Ковальчук) | Функція | `src/review/trend.cpp::xray::review::extractTrendPoint` | Реалізовано | `tests/review/trend_tests.cpp::test_extractTrendPoint_*` |
| D18 | 1 | D (Ковальчук) | Функція | `src/review/trend.cpp::xray::review::splitTrendSegments` | Реалізовано | `tests/review/trend_tests.cpp::test_splitTrendSegments` |
| D19 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::makeReviewExplanation` | Реалізовано | `tests/review/compare_tests.cpp` |
| D25 | 1 | D (Ковальчук) | Функція | `src/review/compare.cpp::xray::review::validateComparisonReport` | Реалізовано | `tests/review/compare_tests.cpp::test_validate_*` |

## A — структура та розбір C++ коду

Облік станом на 2026-10-13 для гілки `a/entity-walk`. Автор A — `chillsmurfkkk`; авторство заголовків E0 та наступних реалізацій підтверджується Git-історією. Стани нижче описують реалізацію та наявні докази, а рішення про зарахування приймає викладач.

| Категорія | Обліковано тут | Правило підрахунку |
|---|---:|---|
| Типи | 22 | Публічні власні класи/структури з окремими рядками нижче; без enum, alias, бібліотечних і тестових типів. |
| Поля | 15 | Облікована вибірка фактично використовуваних полів; це не загальна кількість полів модуля. Успадковані поля враховано лише в базовому класі. |
| Нетривіальні реалізації | 37 | Окремі функції/методи з алгоритмом або перевірками; без конструкторів, getters, декларацій і тестових функцій. |
| Ієрархії | 2 | `SourceProvider` із двома нащадками та `CodeEntity` із трьома. |
| Випадки поліморфізму | 3 | Два динамічні Interface та один шаблонний обхід із двома різними клієнтами. Для `CodeEntity` наявний базовий виклик у тесті функції; демонстрацію всіх трьох нащадків ще слід доповнити. |

### Типи

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A-T01 | 1 | A (chillsmurfkkk) | Тип: Абстрактна сутність; ID, діапазон і дерево дітей | [types.hpp](../src/code/types.hpp): `CodeEntity` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-T02 | 1 | A (chillsmurfkkk) | Тип: Корінь структури файла й утримання оригінальних байтів | [types.hpp](../src/code/types.hpp): `FileEntity` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-T03 | 1 | A (chillsmurfkkk) | Тип: Функція, ключ зіставлення, тіло, статус і керувальне дерево | [types.hpp](../src/code/types.hpp): `FunctionEntity` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declaratorShapes` |
| A-T04 | 1 | A (chillsmurfkkk) | Тип: Клас/структура з кваліфікованим ім'ям та вкладеними сутностями | [types.hpp](../src/code/types.hpp): `TypeEntity` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-T05 | 1 | A (chillsmurfkkk) | Тип: Абстрактний поліморфний Interface збирання файлів | [api.hpp](../src/code/api.hpp): `SourceProvider` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful` |
| A-T06 | 1 | A (chillsmurfkkk) | Тип: Обхід каталогу та передавання вибірки на читання | [api.hpp](../src/code/api.hpp): `DirectoryProvider` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful` |
| A-T07 | 1 | A (chillsmurfkkk) | Тип: Читання явного списку з відбором і дедуплікацією | [api.hpp](../src/code/api.hpp): `FileListProvider` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A-T08 | 1 | A (chillsmurfkkk) | Тип: Результат одного файла; дерево, індекс функцій і діагностика | [types.hpp](../src/code/types.hpp): `ParsedUnit` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-T09 | 1 | A (chillsmurfkkk) | Тип: Причина помилки розбору та її діапазон | [types.hpp](../src/code/types.hpp): `ParseDiagnostic` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `syntaxDiagnosticsHaveConcreteRanges` |
| A-T10 | 1 | A (chillsmurfkkk) | Тип: Напіввідкриті байтові межі й номери рядків від 1 | [types.hpp](../src/code/types.hpp): `SourceRange` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-T11 | 1 | A (chillsmurfkkk) | Тип: Розширення, виключені каталоги та явні шляхи | [types.hpp](../src/code/types.hpp): `FileSelection` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A-T12 | 1 | A (chillsmurfkkk) | Тип: Корінь локального джерела та правила вибірки | [types.hpp](../src/code/types.hpp): `SourceRequest` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `invalidSelectedPathIsSkipped` |
| A-T13 | 1 | A (chillsmurfkkk) | Тип: Відносний шлях, незмінні байти та content ID | [types.hpp](../src/code/types.hpp): `SourceFile` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `sourceBytesArePreserved` |
| A-T14 | 1 | A (chillsmurfkkk) | Тип: Зібране джерело з вибіркою та Coverage | [types.hpp](../src/code/types.hpp): `SourceSnapshot` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `snapshotAndContentIdsBehaveCorrectly` |
| A-T15 | 1 | A (chillsmurfkkk) | Тип: Власне дерево керувальних конструкцій без TSNode | [types.hpp](../src/code/types.hpp): `ControlNode` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `controlTreePreservesBranchLevels` |
| A-T16 | 1 | A (chillsmurfkkk) | Тип: Версії граматики та власної структури | [types.hpp](../src/code/types.hpp): `ParseOptions` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `invalidInputs` |
| A-T17 | 1 | A (chillsmurfkkk) | Тип: Результати файлів з утриманням вихідного знімка | [types.hpp](../src/code/types.hpp): `ParsedSnapshot` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `sourceLifetime` |
| A-T18 | 1 | A (chillsmurfkkk) | Тип: Текст пошуку та необов'язковий фільтр виду | [search.hpp](../src/code/search.hpp): `SearchQuery` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions` |
| A-T19 | 1 | A (chillsmurfkkk) | Тип: Власний ID результату та ознака точного збігу | [search.hpp](../src/code/search.hpp): `SearchMatch` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `searchResultsOwnTheirIds` |
| A-T20 | 1 | A (chillsmurfkkk) | Тип: Пошук через шаблонний обхід; приватні запит і результати | [search.hpp](../src/code/search.hpp): `SearchVisitor` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions` |
| A-T21 | 1 | A (chillsmurfkkk) | Тип: ID сутності та конкретне пояснення суперечності | [validation.hpp](../src/code/validation.hpp): `RangeIssue` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `rangesDetectByteParentViolations` |
| A-T22 | 1 | A (chillsmurfkkk) | Тип: Перевірка діапазонів; приватні розмір джерела й проблеми | [validation.hpp](../src/code/validation.hpp): `RangeValidationVisitor` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `rangesContinueAfterInvalidEntities` |

### Поля

До вибірки не включено `FunctionEntity::parameterCount` і `qualifiers`: вони оголошені, але поточний парсер їх не заповнює. Замість них обліковано реально використовувані `identityKey` і `controlTree`. Просте оголошення поля не є окремою нетривіальною функцією.

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A-P01 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `CodeEntity::id` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `entityIdsAreUniqueAndRepeatable` |
| A-P02 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `CodeEntity::name` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declaratorShapes` |
| A-P03 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `CodeEntity::kind` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions` |
| A-P04 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `CodeEntity::range` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-P05 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `CodeEntity::children` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A-P06 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FileEntity::relativePath` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `originalBytesAndRanges` |
| A-P07 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FileEntity::contentId` | Реалізовано | parseUnit копіює SourceFile::contentId; source:snapshotAndContentIdsBehaveCorrectly перевіряє ID джерел |
| A-P08 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FileEntity::languageId` | Реалізовано | parseUnit встановлює тип FileEntity із типовим languageId = cpp |
| A-P09 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FileEntity::sourceBytes` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `sourceLifetime` |
| A-P10 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FileEntity::parseState` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `syntaxErrorsPreserveValidFunctions` |
| A-P11 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FunctionEntity::signatureTokens` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declaratorTokensIgnoreFormatting` |
| A-P12 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FunctionEntity::bodyRange` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declarationsHaveNoApplicableBody` |
| A-P13 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FunctionEntity::ownerName` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `explicitlyQualifiedMethods` |
| A-P14 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FunctionEntity::identityKey` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `identityIgnoresBodyAndLineShifts` |
| A-P15 | 1 | A (chillsmurfkkk) | Поле | [types.hpp](../src/code/types.hpp): `FunctionEntity::controlTree` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `controlTreeRecognizesLoopsAndLabels` |

### Нетривіальні функції та методи

ID A01–A25 збережено там, де є фактичний відповідник плану; A-F01–A-F20 позначають додаткові реалізації. Один символ враховано один раз. Рядок A17 враховує алгоритм `RangeValidationVisitor::operator()`; `validateEntityRanges` є клієнтом обходу та не збільшує кількість алгоритмів.

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A01 | 1 | A (chillsmurfkkk) | Обхід папки, відбір, Coverage, прогрес і скасування | [source.cpp](../src/code/source.cpp): `DirectoryProvider::collect` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful` |
| A02 | 1 | A (chillsmurfkkk) | Перевірка явних шляхів, дедуплікація, читання й діагностика пропусків | [source.cpp](../src/code/source.cpp): `FileListProvider::collect` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A03 | 1 | A (chillsmurfkkk) | Відхилення абсолютних шляхів і ..; лексична нормалізація | [source.cpp](../src/code/source.cpp): `validateRelativePath` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `invalidSelectedPathIsSkipped` |
| A05 | 1 | A (chillsmurfkkk) | Поблокове читання; UTF-8, BOM, NUL, помилки й скасування | [source.cpp](../src/code/source.cpp): `readFileBytes` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `sourceBytesArePreserved`; `invalidContentIsSkipped` |
| A07 | 1 | A (chillsmurfkkk) | RAII-дерево парсера; структура, діагностика та індекс одного файла | [parse.cpp](../src/code/parse.cpp): `parseUnit` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A08 | 1 | A (chillsmurfkkk) | Обхід синтаксичних вузлів із namespace/type scope та вкладенням сутностей | [parse.cpp](../src/code/parse.cpp): `walkStructure` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A09 | 1 | A (chillsmurfkkk) | Ім'я, scope, токени декларатора, тіло, статус і control tree | [parse.cpp](../src/code/parse.cpp): `extractFunction` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declaratorShapes`; `declarationsHaveNoApplicableBody` |
| A10 | 1 | A (chillsmurfkkk) | Ім'я, діапазон і кваліфіковане ім'я class/struct/union | [parse.cpp](../src/code/parse.cpp): `extractType` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` |
| A11 | 1 | A (chillsmurfkkk) | Розбір явної кваліфікації, глобального scope й спеціальних імен | [parse.cpp](../src/code/parse.cpp): `extractQualifiedName` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `explicitlyQualifiedMethods` |
| A12 | 1 | A (chillsmurfkkk) | Листові токени без коментарів; відхилення ненадійного декларатора | [parse.cpp](../src/code/parse.cpp): `normalizeDeclarator` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declaratorTokensIgnoreFormatting`; `literalContentsRemainSignificant` |
| A13 | 1 | A (chillsmurfkkk) | Побудова власного control tree; else-if/catch і пропуск внутрішніх тіл | [parse.cpp](../src/code/parse.cpp): `appendControlNodes` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `controlTreePreservesBranchLevels`; `controlTreeExcludesInnerBodies` |
| A14 | 1 | A (chillsmurfkkk) | ERROR/MISSING у конкретні причини та діапазони; оновлення Coverage | [parse.cpp](../src/code/parse.cpp): `collectParseDiagnostics` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `syntaxDiagnosticsHaveConcreteRanges`; `missingSyntaxTokenHasPointRange` |
| A15 | 1 | A (chillsmurfkkk) | Перетини діагностики з функцією та not_applicable без тіла | [parse.cpp](../src/code/parse.cpp): `markAnalysisEligibility` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `eligibilityUsesConcreteDiagnosticReasons`; `eligibilityExcludesErrorsBetweenFunctions` |
| A17 | 1 | A (chillsmurfkkk) | Порядок, межі джерела й вкладення байтових та рядкових діапазонів | [validation.hpp](../src/code/validation.hpp): `RangeValidationVisitor::operator()` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `rangesDetectByteParentViolations`; `rangesDetectLineParentViolations` |
| A19 | 1 | A (chillsmurfkkk) | Кодування шляху, кваліфікованого імені й токенів із довжинами UTF-8 | [parse.cpp](../src/code/parse.cpp): `makeIdentityKey` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `identityUsesLengthPrefixedUtf8Fields`; `identityIgnoresBodyAndLineShifts` |
| A22 | 1 | A (chillsmurfkkk) | Запуск SearchVisitor і стабільне ранжування точних збігів перед частковими | [search.hpp](../src/code/search.hpp): `searchEntities` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `searchRanksExactMatchesStably` |
| A25 | 1 | A (chillsmurfkkk) | Власний ітеративний обхід; батьки, порядок, skip_children і stop | [walk.hpp](../src/code/walk.hpp): `walkEntities<Visitor>` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `walkPreservesOrderAndParents`; `walkSkipsOnlySelectedSubtree`; `walkStopsBeforePendingBranches` |
| A-F01 | 1 | A (chillsmurfkkk) | Розрізнення порожнього, відсутнього, нечитабельного кореня й не-каталогу | [source.cpp](../src/code/source.cpp): `validateRoot` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful (коректний корінь)`; `негативні гілки validateRoot потребують окремої перевірки` |
| A-F02 | 1 | A (chillsmurfkkk) | Перевірка компонентів; symlink/junction, вкладений Git і звичайний файл | [source.cpp](../src/code/source.cpp): `validateSourceFile` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `invalidSelectedPathIsSkipped`; `nestedGitTreesAreSkipped` |
| A-F03 | 1 | A (chillsmurfkkk) | Виявлення вкладених Git working trees та помилок читання .git | [source.cpp](../src/code/source.cpp): `nestedGitExclusion` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `nestedGitTreesAreSkipped` |
| A-F04 | 1 | A (chillsmurfkkk) | Перевірка continuation bytes, мінімального code point і діапазонів Unicode | [source.cpp](../src/code/source.cpp): `isValidUtf8` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `invalidContentIsSkipped` |
| A-F05 | 1 | A (chillsmurfkkk) | Детермінований обхід із причинами пропусків, прогресом і скасуванням | [source.cpp](../src/code/source.cpp): `scanDirectory` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `emptyDirectoryIsSuccessful`; `nestedGitTreesAreSkipped`; `cancellationNeverReturnsSnapshot` |
| A-F06 | 1 | A (chillsmurfkkk) | Відбір за каталогами й розширеннями з конкретною причиною виключення | [source.cpp](../src/code/source.cpp): `filterExclusionReason` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `explicitSelectionDeduplicatesAndFilters` |
| A-F07 | 1 | A (chillsmurfkkk) | FNV-1a по оригінальних байтах, hex-кодування й перевірки скасування | [source.cpp](../src/code/source.cpp): `makeContentId` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `snapshotAndContentIdsBehaveCorrectly` |
| A-F08 | 1 | A (chillsmurfkkk) | Ідентифікатор сесії та атомарний лічильник незалежних запусків | [source.cpp](../src/code/source.cpp): `makeSnapshotId` | Реалізовано | [source_provider_test.cpp](../tests/source_provider_test.cpp): `snapshotAndContentIdsBehaveCorrectly` |
| A-F09 | 1 | A (chillsmurfkkk) | RAII створення парсера й обробка несумісної граматики або збою створення | [parse.cpp](../src/code/parse.cpp): `createParser` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `emptyInputs`; `invalidInputs (успішне створення)`; `гілки збою окремо не інжектовано` |
| A-F10 | 1 | A (chillsmurfkkk) | Перевірка байтового буфера/ліміту, розбір, RAII й скасування | [parse.cpp](../src/code/parse.cpp): `parseTree` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `invalidInputs`; `cancellationFromProgress` |
| A-F11 | 1 | A (chillsmurfkkk) | Перевірка меж вузла перед виділенням фрагмента оригінальних байтів | [parse.cpp](../src/code/parse.cpp): `nodeText` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `originalBytesAndRanges`; `invalidInputs` |
| A-F12 | 1 | A (chillsmurfkkk) | Перетворення координат у напіввідкриті байти й endLine останнього токена | [parse.cpp](../src/code/parse.cpp): `nodeRange` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `originalBytesAndRanges`; `controlTreeKeepsOriginalRanges` |
| A-F13 | 1 | A (chillsmurfkkk) | Розрізнення else-if за контекстом та case/default за дочірніми токенами | [parse.cpp](../src/code/parse.cpp): `controlKindFor(TSNode)` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `controlTreePreservesBranchLevels`; `controlTreeRecognizesLoopsAndLabels` |
| A-F14 | 1 | A (chillsmurfkkk) | Збирання значущих токенів і пробілів між словесними компонентами | [parse.cpp](../src/code/parse.cpp): `normalizeName` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `formattedNamespacePreservesIdentity`; `formattedTemplateOwnerPreservesIdentity` |
| A-F15 | 1 | A (chillsmurfkkk) | Пошук імені всередині pointer/reference/parenthesized declarator | [parse.cpp](../src/code/parse.cpp): `findDeclaratorName` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declaratorShapes` |
| A-F16 | 1 | A (chillsmurfkkk) | Відрізнення оголошення функції від pointer/member-pointer declarator | [parse.cpp](../src/code/parse.cpp): `findDeclaredFunction` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `declarationsDistinguishFunctionsFromPointers`; `memberFunctionPointersAreNotDeclarations` |
| A-F17 | 1 | A (chillsmurfkkk) | Пошук умовної компіляції в предках функції | [parse.cpp](../src/code/parse.cpp): `hasConditionalAncestor` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `conditionalAncestorsMarkAllBranches` |
| A-F18 | 1 | A (chillsmurfkkk) | Обхід тіла для виявлення умовної компіляції з підтримкою скасування | [parse.cpp](../src/code/parse.cpp): `hasConditionalDescendant` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `conditionalBodyPreservesControlTree` |
| A-F19 | 1 | A (chillsmurfkkk) | Перевірка входів/версій, складання ParsedSnapshot, Coverage, прогрес і скасування | [parse.cpp](../src/code/parse.cpp): `parse` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `inheritedCoverageAndMultipleFiles`; `progress`; `cancellationKeepsDiagnostics` |
| A-F20 | 1 | A (chillsmurfkkk) | Відбір за назвою/видом без пропуску вкладених функцій; запис exact/partial | [search.hpp](../src/code/search.hpp): `SearchVisitor::operator()` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `searchKindFilterStillVisitsNestedFunctions`; `searchIsCaseSensitiveAndHandlesNoMatches` |

У число 37 не включено прості допоміжні `pathToUtf8`, `isExcludedDirectory`, `controlKindFor(std::string_view)`, `joinScope`, `makeEntityId`, фасад `collect`, обгортку `validateEntityRanges`, реалізації `describe`, getters visitor, defaulted `operator==`, конструктори й деструктори.

### Ієрархії та клієнтський поліморфізм

| ID | ЛР | Автор | Елемент / поведінка | Файл і символ у коді | Стан | Перевірка / демонстрація |
|---|---|---|---|---|---|---|
| A-I01 | 1 | A (chillsmurfkkk) | Ієрархія з 3 класів: SourceProvider → DirectoryProvider, FileListProvider | [api.hpp](../src/code/api.hpp): `SourceProvider, DirectoryProvider, FileListProvider` | Реалізовано | Обидві реалізації collect у [source.cpp](../src/code/source.cpp); сценарії папки та явного списку в [source_provider_test.cpp](../tests/source_provider_test.cpp). |
| A-I02 | 1 | A (chillsmurfkkk) | Ієрархія з 4 класів: CodeEntity → FileEntity, FunctionEntity, TypeEntity | [types.hpp](../src/code/types.hpp): `CodeEntity, FileEntity, FunctionEntity, TypeEntity` | Реалізовано | Абстрактний describe та overrides; [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex`. |
| A-DYN01 | 1 | A (chillsmurfkkk) | Динамічний вибір збирання через SourceProvider& | [source.cpp](../src/code/source.cpp): `collect` | Реалізовано | Фасад викликає `provider.collect` через базове посилання для папки й списку; `emptyDirectoryIsSuccessful` та `explicitSelectionDeduplicatesAndFilters`. |
| A-DYN02 | 1 | A (chillsmurfkkk) | Віртуальний опис через CodeEntity& | [types.hpp](../src/code/types.hpp): `CodeEntity::describe; overrides у нащадках` | Реалізовано | [parser_tests.cpp](../tests/code/parser_tests.cpp): `nestedStructureAndSharedIndex` викликає describe через `const CodeEntity&` для FunctionEntity; показ FileEntity/TypeEntity через ту саму базу — наступна демонстрація. |
| A-STATIC01 | 1 | A (chillsmurfkkk) | Статичний поліморфізм: walkEntities<SearchVisitor> та walkEntities<RangeValidationVisitor> | [walk.hpp](../src/code/walk.hpp): `walkEntities<Visitor>` | Перевірено | Клієнти [search.hpp](../src/code/search.hpp): `searchEntities` і [validation.hpp](../src/code/validation.hpp): `validateEntityRanges`; `searchKindFilterStillVisitsNestedFunctions`, `rangesAcceptParsedEntities`. |

### Межі обліку та перевірка

- A03 відповідає лексичній нормалізації шляхів; перевірка реальних компонентів і symlink/junction окремо облікована в A-F02. Видимість прихованих файлів не налаштовується, тому повний початковий кандидат A04 не позначено виконаним.
- A09 не заявляє підрахунок параметрів і заповнення qualifiers. Умовна компіляція позначається `syntactic_only` у `extractFunction` через A-F17/A-F18; A15 відповідає пересіченню діагностики й функції.
- A17 перевіряє порядок, межі та вкладення координат; не доводить відповідність кожного номера рядка байтовому зміщенню. Окремого індексу рядків A06 немає.
- A16 не внесено як окремий алгоритм: ID призначаються лічильником під час parse/walkStructure, а дубльовані ключі зберігаються для споживача. A18, A20, A21, A23 та A24 окремо не реалізовані й не входять у квоту.
- У SearchVisitor та RangeValidationVisitor стан приватний. Сутності та знімки мають публічні поля й передаються як `shared_ptr<const ...>`; це фактичний поточний дизайн, а не твердження про виконання початкового плану закритих полів сутностей. Інкапсуляцію сутностей ще слід окремо звірити перед захистом.
- Тестові функції не входять у число 37. Для getters, конструкторів, бібліотечного та згенерованого коду немає окремого зарахування. Цей розділ не заявляє виконання GUI ЛР №2 або ретроспективи ЛР №3.

Підтверджений запуск 2026-10-13: `cmake --build --preset windows-x64 --target parser_tests`, потім `ctest --preset windows-x64 -R "^parser$" --output-on-failure`. Пройшли всі 63 сценарії, включно з обходом, пошуком і перевіркою діапазонів. Посилання на source-provider тести вище вказують на наявні сценарії; цей запис не стверджує нового запуску повної збірки або всіх CTest targets.
