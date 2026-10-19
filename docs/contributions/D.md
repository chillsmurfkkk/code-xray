# Внесок D — Ковальчук (`Knuuniacc`)

Дата звірки: 2026-10-19. Код: `6ab06ce31638adc6bbf552932a728fd2145fc677`.
Зона реалізації — `src/review/`, перевірки — `tests/review/`.
Особистий [звіт ЛР №1](../lab1/lab1-D.md), спільні [правила обліку](../contributions.md)
та [результати запуску перевірок](../lab1/lab1-verification.md) доповнюють цей перелік.

## Підсумок і походження

| Категорія | Обліковано | Межі підрахунку |
|---|---:|---|
| Власні типи | 13 | Класи/структури нижче; кожен шаблонний тип один раз, вкладений і локальний типи окремо. |
| Власні поля | 35 | Усі поля цих 13 типів; локальні змінні, бібліотечний стан та успадковані дублікати виключено. |
| Нетривіальні реалізації | 26 | Вибірка з 26 явно перелічених алгоритмів/методів; прості обгортки й перетворення enum на назву виключено. |
| Ієрархії | 2 | Чотири класи правил і три класи експортерів. |
| Випадки поліморфізму | 3 | Два динамічні інтерфейси й власний шаблонний `joinByKey`. |

Початковий `src/review/types.hpp` цілком походить із E0, коміт
[`c8ed63c`](https://github.com/chillsmurfkkk/code-xray/commit/c8ed63c), автор A.
`ComparisonReport`, `ReviewItem`, `MetricDelta`, `TrendPoint` та інші контракти
цього файла використовуються D, але їхні типи й поля не додані до особистих чисел D.
Типи в наступних таблицях створені D після E0; їхнє походження підтверджують
змістовні зміни, а не лише поточний `git blame`.

Основні коміти D:

| Коміт | Внесок |
|---|---|
| [`4227a8f`](https://github.com/chillsmurfkkk/code-xray/commit/4227a8f) | Дві ієрархії, початкові експортери та шаблон зіставлення. |
| [`62f8f4d`](https://github.com/chillsmurfkkk/code-xray/commit/62f8f4d), [`d99f812`](https://github.com/chillsmurfkkk/code-xray/commit/d99f812) | Обробка неоднозначних ключів, повноти й станів порушень. |
| [`12ef0d0`](https://github.com/chillsmurfkkk/code-xray/commit/12ef0d0) | Фасад порівняння, локальний `FunctionRef`, фільтри, сортування й пояснення. |
| [`d4477a0`](https://github.com/chillsmurfkkk/code-xray/commit/d4477a0) | Індексація, класифікація, різниця метрик і виправлення фільтрів. |
| [`dccfc82`](https://github.com/chillsmurfkkk/code-xray/commit/dccfc82), [`268eb79`](https://github.com/chillsmurfkkk/code-xray/commit/268eb79) | Тренди, пропуски, неоднозначність і синтаксичні значення. |
| [`6ab06ce`](https://github.com/chillsmurfkkk/code-xray/commit/6ab06ce) | Модель звіту та реалізація HTML/JSON-експорту із перевірками. |

## Типи

Усі рядки належать D та ЛР №1. Стан «реалізовано» означає наявність відповідного
коду; фактичний запуск тестових цілей зафіксовано окремо в журналі перевірки.

| ID | Тип і відповідальність | Код | Доказ поведінки |
|---|---|---|---|
| D-T01 | `ComparisonRule`: абстрактна перевірка зміни. | [rules.hpp](../../src/review/rules.hpp) | `compare()` викликає правила через базові вказівники. |
| D-T02 | `MetricGrowthRule`: перевірка допустимого приросту. | [rules.hpp](../../src/review/rules.hpp) | [compare_tests.cpp](../../tests/review/compare_tests.cpp), сценарії зміни метрик. |
| D-T03 | `NewFindingRule`: виявлення нових порушень. | [rules.hpp](../../src/review/rules.hpp) | [review_tests.cpp](../../tests/review/review_tests.cpp), блок правил. |
| D-T04 | `ResolvedFindingRule`: відокремлення виправлення від видалення коду. | [rules.hpp](../../src/review/rules.hpp) | `review_tests.cpp`, `revWithResolved` і `revWithRemovedCode`. |
| D-T05 | `ReportExporter`: абстрактний запис звіту. | [exporter.hpp](../../src/review/exporter.hpp) | `exportReport()` і блок експортерів у `review_tests.cpp`. |
| D-T06 | `HtmlExporter`: модель, екранування і шаблон HTML. | [exporter.hpp](../../src/review/exporter.hpp) | [export_tests.cpp](../../tests/review/export_tests.cpp): `test_htmlExporter`. |
| D-T07 | `JsonExporter`: структурований JSON із невідомими значеннями. | [exporter.hpp](../../src/review/exporter.hpp) | `test_jsonExporter`, повторний розбір JSON. |
| D-T08 | `JoinResult<TLeft, TRight>`: зіставлені, відсутні й неоднозначні індекси. | [matching.hpp](../../src/review/matching.hpp) | `review_tests.cpp`, дублікати ключів. |
| D-T09 | `JoinResult::IndexPair`: пара індексів однозначного збігу. | [matching.hpp](../../src/review/matching.hpp) | `joinByKey()` та його клієнти. |
| D-T10 | `EntityReview<KeyType, MetricType>`: типізований результат зіставлення. | [matching.hpp](../../src/review/matching.hpp) | `review_tests.cpp`, звичайні й optional-метрики. |
| D-T11 | `ReportModelRow`: рядок для відображення звіту. | [exporter.hpp](../../src/review/exporter.hpp) | `test_buildReportModel`. |
| D-T12 | `ReportModel`: метадані та підготовлені рядки звіту. | [exporter.hpp](../../src/review/exporter.hpp) | `test_buildReportModel`. |
| D-T13 | Локальний `compare()::FunctionRef`: контекст функції зі знімка. | [compare.cpp](../../src/review/compare.cpp) | `test_source_ref_sourceId_and_entityId_correct`. |

`UnwrapOptional` та його спеціалізація — службові traits, вони не потрібні для
цього обліку й не збільшують число 13. Власний тестовий тип також не додається.

## Поля

Кожен рядок — одне власне поле D. Успадковані поля повторно не рахуються.
Структури результатів мають відкриті поля; поріг поведінкового класу закритий.

| ID | Поле | Код | Використання |
|---|---|---|---|
| D-P01 | `MetricGrowthRule::m_maxAllowedGrowth` | [rules.hpp](../../src/review/rules.hpp) | Поріг `evaluate()`, приватний стан. |
| D-P02 | `JoinResult::matched` | [matching.hpp](../../src/review/matching.hpp) | Однозначні пари індексів. |
| D-P03 | `JoinResult::unmatchedLeft` | [matching.hpp](../../src/review/matching.hpp) | Відсутні відповідники ліворуч. |
| D-P04 | `JoinResult::unmatchedRight` | [matching.hpp](../../src/review/matching.hpp) | Відсутні відповідники праворуч. |
| D-P05 | `JoinResult::ambiguousLeft` | [matching.hpp](../../src/review/matching.hpp) | Дубльовані ключі ліворуч. |
| D-P06 | `JoinResult::ambiguousRight` | [matching.hpp](../../src/review/matching.hpp) | Дубльовані ключі праворуч. |
| D-P07 | `JoinResult::IndexPair::leftIndex` | [matching.hpp](../../src/review/matching.hpp) | Доступ до лівої сутності. |
| D-P08 | `JoinResult::IndexPair::rightIndex` | [matching.hpp](../../src/review/matching.hpp) | Доступ до правої сутності. |
| D-P09 | `EntityReview::key` | [matching.hpp](../../src/review/matching.hpp) | Ключ зіставлення. |
| D-P10 | `EntityReview::kind` | [matching.hpp](../../src/review/matching.hpp) | Вид збігу. |
| D-P11 | `EntityReview::before` | [matching.hpp](../../src/review/matching.hpp) | Попередня метрика або невідоме значення. |
| D-P12 | `EntityReview::after` | [matching.hpp](../../src/review/matching.hpp) | Наступна метрика або невідоме значення. |
| D-P13 | `EntityReview::metricDiff` | [matching.hpp](../../src/review/matching.hpp) | Різниця доступних значень. |
| D-P14 | `EntityReview::findings` | [matching.hpp](../../src/review/matching.hpp) | Зміни порушень для правил. |
| D-P15 | `ReportModelRow::kind` | [exporter.hpp](../../src/review/exporter.hpp) | Вид зміни в HTML. |
| D-P16 | `ReportModelRow::entityKey` | [exporter.hpp](../../src/review/exporter.hpp) | Назва/ключ рядка. |
| D-P17 | `ReportModelRow::path` | [exporter.hpp](../../src/review/exporter.hpp) | Шлях до вихідного файла. |
| D-P18 | `ReportModelRow::metricsSummary` | [exporter.hpp](../../src/review/exporter.hpp) | Текст метрик. |
| D-P19 | `ReportModelRow::findingsSummary` | [exporter.hpp](../../src/review/exporter.hpp) | Текст станів порушень. |
| D-P20 | `ReportModelRow::explanation` | [exporter.hpp](../../src/review/exporter.hpp) | Пояснення зміни. |
| D-P21 | `ReportModelRow::confidence` | [exporter.hpp](../../src/review/exporter.hpp) | Точність зіставлення. |
| D-P22 | `ReportModel::baseId` | [exporter.hpp](../../src/review/exporter.hpp) | Ідентифікатор базового аналізу. |
| D-P23 | `ReportModel::targetId` | [exporter.hpp](../../src/review/exporter.hpp) | Ідентифікатор цільового аналізу. |
| D-P24 | `ReportModel::analysisFingerprint` | [exporter.hpp](../../src/review/exporter.hpp) | Параметри сумісності. |
| D-P25 | `ReportModel::metricSchemaVersion` | [exporter.hpp](../../src/review/exporter.hpp) | Версія схеми метрик. |
| D-P26 | `ReportModel::summary` | [exporter.hpp](../../src/review/exporter.hpp) | Загальний підсумок. |
| D-P27 | `ReportModel::totalItems` | [exporter.hpp](../../src/review/exporter.hpp) | Кількість рядків. |
| D-P28 | `ReportModel::rows` | [exporter.hpp](../../src/review/exporter.hpp) | Рядки моделі. |
| D-P29 | `compare()::FunctionRef::identityKey` | [compare.cpp](../../src/review/compare.cpp) | Ключ для `joinByKey`. |
| D-P30 | `compare()::FunctionRef::entityId` | [compare.cpp](../../src/review/compare.cpp) | Індекс метрик і порушень. |
| D-P31 | `compare()::FunctionRef::relativePath` | [compare.cpp](../../src/review/compare.cpp) | Шлях у `SourceRef`. |
| D-P32 | `compare()::FunctionRef::range` | [compare.cpp](../../src/review/compare.cpp) | Діапазон вихідного коду. |
| D-P33 | `compare()::FunctionRef::sourceId` | [compare.cpp](../../src/review/compare.cpp) | Збереження правильної версії джерела. |
| D-P34 | `compare()::FunctionRef::validity` | [compare.cpp](../../src/review/compare.cpp) | Обробка неповного/синтаксичного аналізу. |
| D-P35 | `compare()::FunctionRef::reason` | [compare.cpp](../../src/review/compare.cpp) | Причина обмеження точності. |

## Нетривіальні реалізації

Усі перелічені реалізації наявні в коді. Докази — сценарії наявних тестів;
підсумок їхнього запуску слід читати в спільному журналі перевірки.

| ID | Символ і поведінка | Код | Перевірка |
|---|---|---|---|
| D01 | `compare`: перевірки входу, індекси, правила, різниці, Coverage і скасування. | [compare.cpp](../../src/review/compare.cpp) | [compare_tests.cpp](../../tests/review/compare_tests.cpp) |
| D02 | `validateCompatibility`: fingerprint, схема й канонічні фільтри. | [compare.cpp](../../src/review/compare.cpp) | `test_validateCompatibility_*`, `test_canonical_file_selection` |
| D03 | `indexEntities`: групування індексів за ключем із дублями. | [matching.hpp](../../src/review/matching.hpp) | `test_indexEntities` |
| D04 | `matchEntities`: збіги, дублікати, optional-метрики й часткові входи. | [matching.hpp](../../src/review/matching.hpp) | [review_tests.cpp](../../tests/review/review_tests.cpp) |
| D05 | `classifyUnmatched`: стан залежно від повноти протилежної сторони. | [matching.hpp](../../src/review/matching.hpp) | `test_classifyUnmatched` |
| D06 | `joinByKey`: власний узагальнений алгоритм однозначного зіставлення. | [matching.hpp](../../src/review/matching.hpp) | `review_tests.cpp`, `compare_tests.cpp` |
| D07 | `computeMetricDelta`: `after - before` лише для двох доступних значень. | [matching.hpp](../../src/review/matching.hpp) | `test_compareMetricValues`, optional-сценарії `review_tests.cpp` |
| D08 | `MetricGrowthRule::evaluate`: наявність різниці й перевищення порогу. | [rules.hpp](../../src/review/rules.hpp) | `compare_tests.cpp`, правила в `compare()` |
| D09 | `NewFindingRule::evaluate`: пошук нового порушення. | [rules.hpp](../../src/review/rules.hpp) | `review_tests.cpp`, `revWithNew` |
| D10 | `ResolvedFindingRule::evaluate`: пошук усуненого порушення. | [rules.hpp](../../src/review/rules.hpp) | `review_tests.cpp`, `revWithResolved` / `revWithRemovedCode` |
| D11 | `matchFindings`: стани new/resolved/existing/removed/unknown. | [matching.hpp](../../src/review/matching.hpp) | `review_tests.cpp`, блок `matchFindings` |
| D12 | `summarizeComparison`: підсумок без повторного рахунку ID. | [compare.cpp](../../src/review/compare.cpp) | `test_summarize` |
| D13 | `groupReviewItems`: стабільне сортування за шляхом, видом і ключем. | [compare.cpp](../../src/review/compare.cpp) | `test_group_sorts_by_path_then_kind` |
| D14 | `filterReviewItems`: поєднання тексту, шляху, виду й точності. | [compare.cpp](../../src/review/compare.cpp) | `test_filter_*`, `test_probe_filter_*` |
| D15 | `rankReviewItems`: порядок за модулем різниці, невідомі наприкінці. | [compare.cpp](../../src/review/compare.cpp) | `test_rank_by_metric_delta` |
| D16 | `buildTrend`: перевірки послідовності аналізів, ліміт і Coverage. | [trend.cpp](../../src/review/trend.cpp) | [trend_tests.cpp](../../tests/review/trend_tests.cpp): `test_buildTrend_*` |
| D17 | `extractTrendPoint`: функція/середнє/максимум із пропусками. | [trend.cpp](../../src/review/trend.cpp) | `test_extractTrendPoint_*` |
| D18 | `splitTrendSegments`: розбиття серії на неперервні ділянки. | [trend.cpp](../../src/review/trend.cpp) | `test_splitTrendSegments` |
| D19 | `makeReviewExplanation`: пояснення змін і обмежень точності. | [compare.cpp](../../src/review/compare.cpp) | `test_probe_syntactic_only_caveat`, unavailable-сценарії |
| D20 | `buildReportModel`: рядки й текстові підсумки для відображення. | [exporter.cpp](../../src/review/exporter.cpp) | [export_tests.cpp](../../tests/review/export_tests.cpp): `test_buildReportModel` |
| D21 | `escapeHtmlText`: обробка HTML-символів. | [exporter.cpp](../../src/review/exporter.cpp) | `test_escapeHtmlText` |
| D22 | `HtmlExporter::write`: підготовка даних, inja, скасування й запис. | [exporter.cpp](../../src/review/exporter.cpp) | `test_htmlExporter` |
| D23 | `JsonExporter::write`: схема, null, Coverage і SourceRef. | [exporter.cpp](../../src/review/exporter.cpp) | `test_jsonExporter` |
| D24 | `writeReportSafely`: тимчасовий файл, підтвердження заміни й відновлення. | [exporter.cpp](../../src/review/exporter.cpp) | `test_writeReportSafely` перевіряє запис/відмову; тести скасування — збереження до заміни. Гілки відновлення після backup окремо не перевірено. |
| D25 | `validateComparisonReport`: ID, джерела, діапазони й допустимі стани. | [compare.cpp](../../src/review/compare.cpp) | `test_validate_*` |
| D-F01 | `exportReport`: валідація шляху, вибір експортера, Error/Cancelled/receipt. | [exporter.cpp](../../src/review/exporter.cpp) | `test_exportReport_html_and_json` |

Початковий кандидат D07 `compareMetricValues` є обгорткою над
`computeMetricDelta`; обидва не рахуються окремо. D07 тут позначає саме алгоритм.
Не включені `toOptionalMetric`, `evaluateFinding`, `name`, конструктори,
деструктори, службові `metricName`/`ruleName`, лямбди всередині вже облікованих
алгоритмів і альтернативний `exporter_stub.cpp`.

## Ієрархії та поліморфізм

| ID | Реалізація | Клієнт і доказ |
|---|---|---|
| D-I01 | `ComparisonRule` → `MetricGrowthRule`, `NewFindingRule`, `ResolvedFindingRule` | [compare.cpp](../../src/review/compare.cpp): реєстр `unique_ptr<ComparisonRule>` і віртуальний `evaluate`. |
| D-I02 | `ReportExporter` → `HtmlExporter`, `JsonExporter` | [exporter.cpp](../../src/review/exporter.cpp): `exportReport`, виклик `exporter->write`. |
| D-DYN01 | Вибір реалізації правила під час виконання. | `compare()`; прямий виклик через `ComparisonRule*` також є в `review_tests.cpp`. |
| D-DYN02 | Вибір формату через базовий інтерфейс експорту. | `exportReport()` і `review_tests.cpp` для двох експортерів при ввімкненому експорті. |
| D-STATIC01 | `joinByKey<TLeft, TRight, KeyFn>` із різними типами даних. | `compare()` зіставляє `FunctionRef`; `matchFindings()` — порушення; `review_tests.cpp` перевіряє сутності й порушення. |

## Межі обліку

`MetricGrowthRule` приховує поріг, локальний `FunctionRef` не виходить із
`compare()`. Відкриті поля `EntityReview`, `JoinResult` і моделей звіту є фактичним
дизайном записів даних; цей реєстр не називає їх приватними. ЛР №2, готовність GUI
та особиста ретроспектива inja не заявляються виконаними. Перевірку first-parent
ланцюжка має забезпечити координатор C; D не читає Git і сам її не доводить.
