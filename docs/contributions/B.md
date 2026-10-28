# Реєстр фактичного внеску B — ЛР №1

Автор реалізацій: Kolya0685. Аудит охоплює код на `6ab06ce31638adc6bbf552932a728fd2145fc677`, підготовлено 2026-10-19. Повний звіт про мету модуля, рішення ООП, обробку помилок і межі виконання: [lab1-B.md](../lab1/lab1-B.md).

Облік не приписує B декларації `AnalysisProfile`, `MetricResult`, `Finding` або `AnalysisSnapshot`: їх додано в E0 коміті `c8ed63c17be2dbaaa0dc2ccdeccc6c6106e1573f` автором A (`chillsmurfkkk`). B розширив їх використання, але не створив ці контракти. Наведені нижче типи, поля та реалізації утворюють явну облікову вибірку; додатковий власний RAII-помічник окремо зазначений після неї.

Підсумок вибірки: **обліковано 13 типів, 19 полів, 34 нетривіальні реалізації, 2 ієрархії та 3 випадки поліморфізму**; рішення про зарахування приймає викладач. Статус елементів вибірки — **Реалізовано** на знімку коду. Suite `analysis`, `analysis_engine` і `profiles` пройшли у спільній Windows-перевірці з [lab1-verification.md](../lab1/lab1-verification.md); для кожної функції нижче вказаний її тестовий suite.

## Обліковані типи — 13

Статус кожного типу — **Реалізовано**; файли з оголошеннями вказані в таблиці, а поведінку реалізацій перевіряють suite у наступній таблиці.

| ID | Тип | Файл / символ | Походження та роль |
|---|---|---|---|
| B-T01 | Абстрактний клас `Metric` | [metrics.hpp](../../src/analysis/metrics.hpp) | Базовий інтерфейс `id`, `name`, `calculate`; перша ієрархія. |
| B-T02 | `LengthMetric` | [metrics.hpp](../../src/analysis/metrics.hpp) | Обчислює довжину функції за рядковим діапазоном. |
| B-T03 | `NestingMetric` | [metrics.hpp](../../src/analysis/metrics.hpp) | Обчислює максимальний рівень вкладених керувальних конструкцій. |
| B-T04 | `BranchCountMetric` | [metrics.hpp](../../src/analysis/metrics.hpp) | Рахує конструкції, визначені політикою метрики. |
| B-T05 | Абстрактний клас `Rule` | [rules.hpp](../../src/analysis/rules.hpp) | Базовий інтерфейс перевірки через `evaluate`; друга ієрархія. |
| B-T06 | `LongFunctionRule` | [rules.hpp](../../src/analysis/rules.hpp) | Перевіряє поріг довжини функції. |
| B-T07 | `DeepNestingRule` | [rules.hpp](../../src/analysis/rules.hpp) | Перевіряє поріг вкладеності. |
| B-T08 | `ManyBranchesRule` | [rules.hpp](../../src/analysis/rules.hpp) | Перевіряє поріг розгалужень. |
| B-T09 | `AggregateResult` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Значення агрегату, знаменники та сутності з максимумом. |
| B-T10 | `MeanReducer` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Приховує стан онлайн-середнього за методом накопичення. |
| B-T11 | `MaxReducer` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Приховує максимум і його сутності за методом накопичення. |
| B-T12 | `CoverageSummary` | [engine.hpp](../../src/analysis/engine.hpp) | Рахує стани придатності функцій і зберігає Coverage джерела. |
| B-T13 | `FindingFilter` | [engine.hpp](../../src/analysis/engine.hpp) | Передає критерії відбору findings. |

## Обліковані поля — 19

Статус кожного поля — **Реалізовано** у власному типі B; відповідні агрегатні й фільтраційні сценарії входять до пройдених suite `analysis_engine`.

| ID | Власний тип | Поле | Файл | Пояснення |
|---|---|---|---|---|
| B-F01 | `AggregateResult` | `value` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Необов'язкове числове значення для порожнього або непорожнього агрегату. |
| B-F02 | `AggregateResult` | `included` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Кількість врахованих числових результатів. |
| B-F03 | `AggregateResult` | `unknown` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Кількість невідомих або непридатних для числа результатів. |
| B-F04 | `AggregateResult` | `notApplicable` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Окремо рахує метрики, до яких функція не застосовується. |
| B-F05 | `AggregateResult` | `syntacticOnly` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Показує кількість синтаксично неповних, але числових результатів. |
| B-F06 | `AggregateResult` | `maxima` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Список ID всіх сутностей, що мають максимальне значення. |
| B-F07 | `MeanReducer` | `mean_` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Приватне проміжне середнє для онлайн-алгоритму. |
| B-F08 | `MeanReducer` | `count_` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Приватний знаменник, що не включає невідомі значення. |
| B-F09 | `MaxReducer` | `maximum_` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Приватний максимум або відсутність значення для порожньої вибірки. |
| B-F10 | `MaxReducer` | `entities_` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | Приватний список сутностей з поточним максимумом. |
| B-F11 | `CoverageSummary` | `valid` | [engine.hpp](../../src/analysis/engine.hpp) | Кількість повністю валідних функцій. |
| B-F12 | `CoverageSummary` | `syntacticOnly` | [engine.hpp](../../src/analysis/engine.hpp) | Кількість функцій із синтаксичним обмеженням точності. |
| B-F13 | `CoverageSummary` | `notApplicable` | [engine.hpp](../../src/analysis/engine.hpp) | Кількість функцій без застосовного тіла. |
| B-F14 | `CoverageSummary` | `unavailable` | [engine.hpp](../../src/analysis/engine.hpp) | Кількість функцій, недоступних через помилки парсера. |
| B-F15 | `CoverageSummary` | `coverage` | [engine.hpp](../../src/analysis/engine.hpp) | Зберігає спільне покриття та діагностику розбору. |
| B-F16 | `FindingFilter` | `rule` | [engine.hpp](../../src/analysis/engine.hpp) | Необов'язкове обмеження одним правилом. |
| B-F17 | `FindingFilter` | `pathContains` | [engine.hpp](../../src/analysis/engine.hpp) | Підрядок відносного шляху для фільтрації. |
| B-F18 | `FindingFilter` | `textContains` | [engine.hpp](../../src/analysis/engine.hpp) | Підрядок пояснення або опису функції. |
| B-F19 | `FindingFilter` | `validity` | [engine.hpp](../../src/analysis/engine.hpp) | Необов'язковий фільтр за точністю результату. |

## Обліковані нетривіальні реалізації — 34

Остання колонка вказує suite з наявними тестами відповідної поведінки. Спільний Windows запуск цих suite пройшов; точні команди та повний журнал наведені в [lab1-verification.md](../lab1/lab1-verification.md).

| ID | Реалізація | Код | Перевірка / поведінка |
|---|---|---|---|
| B01 | `LengthMetric::calculate` | [metrics.cpp](../../src/analysis/metrics.cpp) | [analysis_tests](../../tests/analysis_tests.cpp); unavailable, declaration, valid і syntactic-only. |
| B02 | `NestingMetric::calculate` | [metrics.cpp](../../src/analysis/metrics.cpp) | [analysis_tests](../../tests/analysis_tests.cpp); вкладеність і стани без тіла/точності. |
| B03 | `BranchCountMetric::calculate` | [metrics.cpp](../../src/analysis/metrics.cpp) | [analysis_tests](../../tests/analysis_tests.cpp); підрахунок вузлів і відтинання піддерев. |
| B04 | `LongFunctionRule::evaluate` | [rules.cpp](../../src/analysis/rules.cpp) | Forwarder до спільного алгоритму; є в плані, але не включено до 34. |
| B05 | `DeepNestingRule::evaluate` | [rules.cpp](../../src/analysis/rules.cpp) | Forwarder до спільного алгоритму; є в плані, але не включено до 34. |
| B06 | `ManyBranchesRule::evaluate` | [rules.cpp](../../src/analysis/rules.cpp) | Forwarder до спільного алгоритму; є в плані, але не включено до 34. |
| B07 | `analyze` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); запуск, прогрес, скасування, реєстри й повний результат. |
| B08 | `validateProfile` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); версія, межі, текст, UTF-8. |
| B09 | `decodeProfile` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); схема, типи, невідомі/дубльовані ключі, пошкоджений JSON. |
| B10 | `encodeProfile` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); валідний формат, стабільний round-trip, помилки серіалізації. |
| B11 | `loadProfile` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); відсутній файл, помилки читання й JSON. |
| B12 | `saveProfile` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); атомарна заміна, невдалий шлях, збереження старого файла. |
| B13 | `duplicateProfile` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); новий ID, незалежність копії та невалідна назва. |
| B14 | `makeAnalysisFingerprint` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); параметри сумісності й канонічні списки. |
| B15 | `buildMetricRegistry` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); повний набір потрібних метрик і унікальні ID. |
| B16 | `resolveEnabledRules` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); налаштування профілю та залежність правила від метрики. |
| B17 | `summarizeCoverage` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); окремий облік валідних, syntactic-only, unavailable, N/A. |
| B18 | `aggregateMetrics<Reducer>` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | [engine_tests](../../tests/analysis/engine_tests.cpp), [demo.cpp](../../tests/analysis/demo.cpp); середнє/максимум, пропуски й нічиї. |
| B19 | `MeanReducer::accumulate` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | [engine_tests](../../tests/analysis/engine_tests.cpp), [demo.cpp](../../tests/analysis/demo.cpp); онлайн-середнє без цілочисельної суми. |
| B20 | `MaxReducer::accumulate` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | [engine_tests](../../tests/analysis/engine_tests.cpp), [demo.cpp](../../tests/analysis/demo.cpp); максимум і всі пов'язані сутності. |
| B21 | `filterFindings` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); комбіновані фільтри правила, точності, шляху й тексту. |
| B22 | `groupFindingsByFile` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); групи за шляхом і стабільний порядок. |
| B23 | `rankFunctions` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); порядок за значенням, tie-break та unknown last. |
| B24 | `formatFindingExplanation` | [rules.cpp](../../src/analysis/rules.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp), [analysis_tests](../../tests/analysis_tests.cpp); одиниця, поріг, шлях/рядок, точність. |
| B25 | `validateAnalysisResult` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); некоректні посилання, дублікати, пропуски й суперечливі findings. |
| B26 | `isNestingConstruct` | [metrics.cpp](../../src/analysis/metrics.cpp) | [analysis_tests](../../tests/analysis_tests.cpp); класифікація конструкцій, що збільшують глибину. |
| B27 | `computeMaxNesting` | [metrics.cpp](../../src/analysis/metrics.cpp) | [analysis_tests](../../tests/analysis_tests.cpp); рекурсивний обхід із правилами вкладеності та відсіканням. |
| B28 | `computeBranchCount` | [metrics.cpp](../../src/analysis/metrics.cpp) | [analysis_tests](../../tests/analysis_tests.cpp); рекурсивний підрахунок і пропуск лямбд/локальних типів. |
| B29 | `evaluateThreshold` | [rules.cpp](../../src/analysis/rules.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp), [analysis_tests](../../tests/analysis_tests.cpp); спільна перевірка порогу, узгодженості та точності. |
| B30 | `validLabel` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); непорожній текст без керувальних символів. |
| B31 | `requireKeys` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); об'єкт JSON із точним набором полів. |
| B32 | `nonnegativeInteger` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); тип, нижня межа й переповнення int64. |
| B33 | `uniqueId` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); новий ID при дублюванні профілю. |
| B34 | `isAnalysisCurrent` | [profiles.cpp](../../src/analysis/profiles.cpp) | [profile_tests](../../tests/analysis/profile_tests.cpp); зміна параметрів робить аналіз застарілим. |
| B35 | `functions` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); збір лише наявних функцій з одиниць розбору. |
| B36 | `functionIndex` | [engine.cpp](../../src/analysis/engine.cpp) | [engine_tests](../../tests/analysis/engine_tests.cpp); індекс сутностей з урахуванням відсутнього ParsedSnapshot. |
| B37 | `MaxReducer::finish` | [aggregation.hpp](../../src/analysis/aggregation.hpp) | [engine_tests](../../tests/analysis/engine_tests.cpp), [demo.cpp](../../tests/analysis/demo.cpp); упорядковує ID усіх сутностей з максимальним значенням. |

B04–B06 показані для звірки з початковим планом, але не входять до підсумку 34: це три короткі override-методи, що лише передають різний поріг до `evaluateThreshold`. Прості helper-и `invalid`/`fileError`, `MeanReducer::finish`, базові `id`/`name`/`targetMetricId`, конструктори, getters, тестові функції та самі декларації виключені. `MaxReducer::finish` врахований, бо впорядковує всі ID з максимальним значенням; алгоритм перевірки правила також порахований один раз як B29.

## Додатковий власний RAII-помічник поза обліковою вибіркою

Тип `TemporaryDirectory` із `profiles.cpp`, його поле `path` та деструктор є власною реалізацією B, але не входять до вибіркових підсумків вище. Деструктор прибирає тимчасовий каталог після успішного або невдалого збереження профілю; сценарій очищення перевіряє [profile_tests](../../tests/analysis/profile_tests.cpp), який пройшов у спільній перевірці.

| ID поза підсумком | Елемент | Код | Стан / доказ |
|---|---|---|---|
| B-T14 | `TemporaryDirectory` | [profiles.cpp](../../src/analysis/profiles.cpp) | Реалізовано; внутрішня RAII-обгортка збереження. |
| B-F20 | `TemporaryDirectory::path` | [profiles.cpp](../../src/analysis/profiles.cpp) | Реалізовано; поле тимчасового каталогу, очищення перевіряє [profile_tests](../../tests/analysis/profile_tests.cpp). |
| B38 | `TemporaryDirectory::~TemporaryDirectory` | [profiles.cpp](../../src/analysis/profiles.cpp) | Реалізовано; прибирає тимчасовий каталог, очищення перевіряє [profile_tests](../../tests/analysis/profile_tests.cpp). |

## Ієрархії та клієнти поліморфізму

| Випадок | Ієрархія / механізм | Реальний клієнт |
|---|---|---|
| Динамічний 1 | `Metric` → `LengthMetric`, `NestingMetric`, `BranchCountMetric` | `analyze()` обходить registry як `const Metric&` і викликає `calculate()` для кожної функції. |
| Динамічний 2 | `Rule` → `LongFunctionRule`, `DeepNestingRule`, `ManyBranchesRule` | `analyze()` обходить увімкнені правила як `const Rule&` і викликає `evaluate()` для відповідного metric ID. |
| Статичний | `aggregateMetrics<Reducer>` із `MeanReducer` і `MaxReducer` | [demo.cpp](../../tests/analysis/demo.cpp) та engine tests запускають один шаблонний алгоритм із двома типами reducer. |

## Авторство та межі доказу

Власні реалізації надходять із чотирьох авторських комітів Kolya0685: `5fb5a31c906bede7f777bbd2cbe718a348d4611f` (довжина та перше правило), `6130f62248aa2b6b830c44b6fa45ac4b985789be` (дві метрики), `327d2921276f6d6365cddb084587d45e09255a69` (правила й JSON-профілі), `0387bbd78deba94609b2fb98a78c4673adee98bd` (engine та агрегати). E0 типи в `src/analysis/types.hpp` походять від `c8ed63c17be2dbaaa0dc2ccdeccc6c6106e1573f` автора A; вони не включені в рахунок B.

Тестові сценарії та команди для `analysis`, `analysis_engine`, `profiles` наведені в [analysis-engine.md](../analysis-engine.md). Демо фактично повернуло 3 метрики, 1 finding, середнє й максимум 61 при порозі 60; його журнал і результат спільного запуску тестів наведені в [lab1-verification.md](../lab1/lab1-verification.md).

## ЛР №2 — інтеграція форм B

Від `b6a7ade`: **4 форми, 22 власні контроли, 4 контейнери, 12 обробників**.
Переглядач CodeView A, спільна навігація, індикатор прогресу та скасування
окремо до кількості контролів форм не включені.

| Форма в `src/ui/analysis/` | Контроли (objectName) | Кількість |
|---|---|---:|
| ProfilesPage | `profilesList`; `createProfile`, `duplicateProfile`, `importProfile`, `saveProfile` | 5 |
| MetricsPage | `metricsTable`; `metricSort`, `metricDirection`, `aggregation`; `metricPath`; `showUnknown` | 6 |
| RulesPage | `rulesTable`; `threshold0..2`; `applyRules` | 5 |
| FindingsPage | `findingsTable`; `findingRule`; `findingSearch`, `findingPath`; `navigateFinding`, `reanalyze` | 6 |

Контейнери — список профілів і три таблиці. Вкладені прапорці не рахуються окремо.

| Обробники | Події |
|---|---|
| ProfilesPage::onProfileCreated / onProfileDuplicated / onProfileImported / onProfileSaved | Чотири кнопки профілів. |
| Session::selectProfile | Вибір профілю. |
| MetricsPage::refresh / updateAggregate | Сортування, фільтри й агрегація. |
| RulesPage::readDraft / onRulesApplied | Пороги, перемикачі та застосування. |
| FindingsPage::refresh / onFindingActivated | Фільтри та код порушення. |
| Session::requestAnalysis | Повторний аналіз. |

Перевірка 2026-10-28: 20/20 цілей CTest та нативний smoke-test на macOS із
Qt 6.11.2. У тестах реальні C++ файли передаються до спільного головного вікна;
перевірено обидві теми, навігацію A/B/D, скасування та заміну джерела під час
аналізу. [Команди й межі перевірки](../analysis-ui.md).
