# Фактичний внесок C: Git та історія

## Мета і виконана частина

Звіт описує код у snapshot `6ab06ce31638adc6bbf552932a728fd2145fc677` від 2026-10-19. Виконана частина C — дві ієрархії запитів і вибору комітів, їхні операції над переданими записами та шаблонне групування. Код міститься у [`src/history/types.hpp`](../../src/history/types.hpp), а клієнтські приклади й assertions — у [`tests/history_test.cpp`](../../tests/history_test.cpp).

Облік зроблено за повним diff від E0, а не лише за `git blame`, оскільки форматування змінювало частину історії рядків. C має **7 власних типів, 7 полів і 7 нетривіальних реалізацій**. Початковий `HistoryRequest` із тими самими чотирма полями, `RepositorySpec`, `CommitRecord`, `ChangeRecord`, `HistoryResult`, спільні `Result` і `JobContext`, а також `Error`, `Coverage` та інші контракти походять з E0 (`c8ed63c`, автор A) і не зараховуються C. `Oid` є alias і також не рахується типом.

## Відповідність вимогам

| Вимога ЛР №1 | Фактично в C | Різниця до мінімуму |
|---|---:|---:|
| Класи або подібні типи | 7 | бракує 2 до 9 |
| Поля | 7 | бракує 8 до 15 |
| Нетривіальні методи або функції | 7 | бракує 18 до 25 |
| Ієрархії успадкування | 2; одна містить 4 класи, інша 3 | мінімум виконано |
| Незалежні поліморфні випадки | 2 динамічні через базові типи та 1 статичний шаблонний | мінімум виконано |

### Типи

| ID | Тип C | Файл і символ | Стан та доказ |
|---|---|---|---|
| C-T01 | `HistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `HistoryQuery` | Реалізовано як абстрактний базовий клас із віртуальним `apply`. |
| C-T02 | `CommitRangeQuery` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Реалізовано відбір меж у списку комітів. |
| C-T03 | `FileHistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Реалізовано фільтр за старим або новим шляхом у змінах. |
| C-T04 | `RevisionSelection` | [`types.hpp`](../../src/history/types.hpp), `RevisionSelection` | Реалізовано як абстрактний базовий клас із віртуальним `select`. |
| C-T05 | `LastNSelection` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Реалізовано вибір перших N записів. |
| C-T06 | `ExplicitSelection` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Реалізовано вибір заданих OID із перевіркою та усуненням дублікатів. |
| C-T07 | `PeriodicSelection` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Реалізовано вибірку з кроком; нульовий крок відхиляється конструктором. |

### Поля

| ID | Поле C | Файл і символ | Стан та доказ |
|---|---|---|---|
| C-F01 | `CommitRangeQuery::startOid` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Реалізовано як приватне поле початкової межі. |
| C-F02 | `CommitRangeQuery::endOid` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Реалізовано як приватне поле кінцевої межі. |
| C-F03 | `FileHistoryQuery::filePath` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Реалізовано як приватний шлях для фільтра. |
| C-F04 | `FileHistoryQuery::allChanges` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Реалізовано як приватний список змін для фільтрації. |
| C-F05 | `LastNSelection::count` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Реалізовано як приватна кількість вибраних комітів. |
| C-F06 | `ExplicitSelection::targetOids` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Реалізовано як приватний список цільових OID. |
| C-F07 | `PeriodicSelection::step` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Реалізовано як приватний крок; конструктор гарантує значення більше нуля. |

### Нетривіальні реалізації

| ID | Функція або метод | Файл і символ | Стан та доказ |
|---|---|---|---|
| C06 | `CommitRangeQuery::apply` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery::apply` | Реалізовано перевірку наявності меж та інверсного порядку; результат містить включний діапазон. |
| C07 | `FileHistoryQuery::apply` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery::apply` | Реалізовано зіставлення шляху з `oldPath` або `newPath`. |
| C17 | `LastNSelection::select` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection::select` | Реалізовано вибір перших N комітів у вхідному порядку. |
| C18 | `ExplicitSelection::select` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection::select` | Реалізовано перевірку OID, згортання дублікатів і збереження порядку вхідного списку; невідомий OID спричиняє `std::invalid_argument`. |
| C19 | `PeriodicSelection::select` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection::select` | Реалізовано вибір записів з індексами `0, step, 2*step, ...`. |
| C21 | `groupRecords<Record, KeyFn>` | [`types.hpp`](../../src/history/types.hpp), `groupRecords` | Реалізовано шаблонне групування за ключем зі збереженням порядку появи груп. |
| C-IM01 | `PeriodicSelection::PeriodicSelection` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection::PeriodicSelection` | Реалізовано перевірку інваріанта `step != 0`; нульовий крок спричиняє `std::invalid_argument`. |

## Інкапсуляція

Поля параметрів стратегій закриті через `private`, а доступ до поведінки наданий методами `apply` і `select`. Типи записів E0 залишаються простими структурами даних і не зараховуються C. Для наведених у цьому обліку семи власних полів рівень доступу обмежений відповідно до їхньої ролі в стратегіях.

## Ієрархії та поліморфні клієнти

Перша ієрархія — `HistoryQuery` з `CommitRangeQuery` і `FileHistoryQuery`, разом три класи. Друга — `RevisionSelection` з `LastNSelection`, `ExplicitSelection` і `PeriodicSelection`, разом чотири класи.

У тестовому клієнті `executeQuery(const HistoryQuery&, ...)` викликає `apply` через базовий тип, а `executeSelection(const RevisionSelection&, ...)` викликає `select` через інший базовий тип; це два динамічні випадки. `groupRecords` викликається для `CommitRecord` із ключем автора та для `ChangeRecord` із ключем статусу, що демонструє статичний поліморфізм узагальненого алгоритму. Разом у коді показано три незалежні випадки.

## Відтворення і демонстрація

Спільний журнал [перевірки коду для звітів ЛР №1](../lab1/lab1-verification.md) фіксує успішну збірку Windows x64 preset `windows-x64` і успішне завершення всіх 13 цілей CTest із доступним тимчасовим каталогом під `build/`. Ціль `history_test` пройшла. Її [`tests/history_test.cpp`](../../tests/history_test.cpp) використовує чотири mock-коміти та mock-зміни, перевіряє включні межі діапазону, помилки відсутньої та інвертованої межі, історію файла, три стратегії вибору, дублікати, крок 0 і групування за автором та статусом. Результат підтверджує ці сценарії на контрольних записах; він не перевіряє читання реального Git.

## Висновок і межі роботи

Для ЛР №1 реалізована частина з двома ієрархіями та трьома прикладами поліморфізму, але кількісні пороги не досягнуті: бракує 2 типів, 8 полів і 18 нетривіальних реалізацій. [Звіт ЛР №1 для C](../lab1/lab1-C.md) містить короткий перелік і приклад очікуваної поведінки.

У [`src/history/api.hpp`](../../src/history/api.hpp) `query` і `loadSnapshot` лише оголошені, визначень у snapshot немає. У `src/history/` немає реалізації libgit2: відкриття репозиторію, розв'язання ref/OID, first-parent обходу, читання tree/blob, diff/patch, поведінки root/merge чи формування справжнього `SourceSnapshot`. `CommitRangeQuery` і `FileHistoryQuery` працюють із уже переданими записами. Форми й обробники GUI, координатор фонових запусків та ретроспектива libgit2 у цьому snapshot також не реалізовані; ЛР №2 і інтеграція ЛР №3 не заявляються завершеними.

Зміни, що сформували облікований код, видно в історії Git. `d9ee56d` започаткував C модульні типи та вибір комітів; `afbc345` переробив це у поточні ієрархії `HistoryQuery`/`RevisionSelection` і додав початковий history-тест; `1c4555b` замінив суміжне групування на групування за ключем і функцією-екстрактором. `4ce319a` змінив обробку меж та вибірки; `49bcc14` додав явні помилки некоректних меж/OID і валідацію кроку `PeriodicSelection`; `95e3cf6` перевіряє відсутній OID до побудови вибірки та прибирає дублікати, зберігаючи порядок списку комітів; `7a7fa20` додав перевірку інверсного діапазону й випадку порожнього списку. `HistoryRequest` повернувся до файла в `1c4555b`, але ті самі тип і поля були в E0 до цього ланцюжка змін, тому до власних типів або полів C не зараховані.
