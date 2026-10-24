# Фактичний внесок C: Git та історія

## Мета і виконана частина

Звіт описує код у snapshot `688d11833280c3228c4e0e695ff593b371aa428a` (з урахуванням реалізації регістронезалежних фільтрів, обробки прапорців `strictMatch`/`allowOverflow` та розширених модульних тестів). Виконана частина C — дві ієрархії запитів і вибору комітів, їхні операції над переданими записами та шаблонне групування. Код міститься у [`src/history/types.hpp`](../../src/history/types.hpp), а клієнтські приклади й assertions — у [`tests/history_test.cpp`](../../tests/history_test.cpp).

Облік зроблено за повним diff від E0, а не лише за `git blame`. Початковий `HistoryRequest`, `RepositorySpec`, `CommitRecord`, `ChangeRecord`, `HistoryResult` та спільні контракти походять з E0 (`c8ed63c17be2dbaaa0dc2ccdeccc6c6106e1573f`, автор A) і не зараховуються C. `Oid` є alias і також не рахується типом.

## Відповідність вимогам

| Вимога ЛР №1 | Фактично в C | Статус |
|---|---:|---|
| Класи або подібні типи | 11 | Виконано (очікувалося >= 9) |
| Поля | 23 | Виконано (очікувалося >= 15) |
| Нетривіальні методи або функції | 19 | Частково / відповідає поточній межі PR (19 із 25, залишається 6) |
| Ієрархії успадкування | 2 ієрархії (5 та 6 класів із базовими) | Виконано |
| Незалежні поліморфні випадки | 2 динамічні через базові типи та 1 статичний шаблонний | Виконано |

### Типи

| ID | Тип C | Файл і символ | Стан та доказ |
|---|---|---|---|
| C-T01 | `HistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `HistoryQuery` | Реалізовано як абстрактний базовий клас із віртуальним `apply`. |
| C-T02 | `CommitRangeQuery` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Реалізовано відбір меж у списку комітів із валідацією. |
| C-T03 | `FileHistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Реалізовано фільтр за старим або новим шляхом (з підтримкою `strictMatch`). |
| C-T04 | `AuthorHistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `AuthorHistoryQuery` | Реалізовано фільтрацію за автором/email із підтримкою `caseSensitive`. |
| C-T05 | `MessageKeywordQuery` | [`types.hpp`](../../src/history/types.hpp), `MessageKeywordQuery` | Реалізовано фільтрацію за ключовим словом із підтримкою `matchCase`. |
| C-T06 | `RevisionSelection` | [`types.hpp`](../../src/history/types.hpp), `RevisionSelection` | Реалізовано як абстрактний базовий клас із віртуальним `select`. |
| C-T07 | `LastNSelection` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Реалізовано вибір перших N записів (із обробкою `allowOverflow`). |
| C-T08 | `ExplicitSelection` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Реалізовано вибір заданих OID із дедуплікацією та перевіркою `strictExistence`. |
| C-T09 | `PeriodicSelection` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Реалізовано вибірку з кроком та зміщенням; нульовий крок відхиляється. |
| C-T10 | `AuthorSelection` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Реалізовано вибірку ревізій за автором з урахуванням `caseSensitive`. |
| C-T11 | `DateRangeSelection` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Реалізовано вибірку комітів за часовим діапазоном `authoredAt` (`from <= to`). |

### Поля

| ID | Поле C | Файл і символ | Стан та доказ |
|---|---|---|---|
| C-F01 | `CommitRangeQuery::startOid` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Приватне поле початкової межі. |
| C-F02 | `CommitRangeQuery::endOid` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Приватне поле кінцевої межі. |
| C-F03 | `FileHistoryQuery::filePath` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Приватний шлях для фільтра. |
| C-F04 | `FileHistoryQuery::allChanges` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Приватний список змін для фільтрації. |
| C-F05 | `FileHistoryQuery::strictMatch` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Прапорець суворого збігу шляху. |
| C-F06 | `AuthorHistoryQuery::targetAuthor` | [`types.hpp`](../../src/history/types.hpp), `AuthorHistoryQuery` | Цільовий автор для запиту. |
| C-F07 | `AuthorHistoryQuery::caseSensitive` | [`types.hpp`](../../src/history/types.hpp), `AuthorHistoryQuery` | Прапорець врахування регістру. |
| C-F08 | `AuthorHistoryQuery::maxResults` | [`types.hpp`](../../src/history/types.hpp), `AuthorHistoryQuery` | Максимальна кількість результатів. |
| C-F09 | `MessageKeywordQuery::keyword` | [`types.hpp`](../../src/history/types.hpp), `MessageKeywordQuery` | Ключове слово для пошуку в повідомленні. |
| C-F10 | `MessageKeywordQuery::matchCase` | [`types.hpp`](../../src/history/types.hpp), `MessageKeywordQuery` | Прапорець чутливості до регістру. |
| C-F11 | `MessageKeywordQuery::minMessageLength` | [`types.hpp`](../../src/history/types.hpp), `MessageKeywordQuery` | Мінімальна довжина повідомлення. |
| C-F12 | `LastNSelection::count` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Кількість вибраних комітів. |
| C-F13 | `LastNSelection::allowOverflow` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Прапорець дозволу переповнення. |
| C-F14 | `ExplicitSelection::targetOids` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Список цільових OID. |
| C-F15 | `ExplicitSelection::strictExistence` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Сувора перевірка наявності OID. |
| C-F16 | `PeriodicSelection::step` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Крок вибірки. |
| C-F17 | `PeriodicSelection::offset` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Початкове зміщення вибірки. |
| C-F18 | `AuthorSelection::authorName` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Ім'я автора для вибірки. |
| C-F19 | `AuthorSelection::matchEmailToo` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Прапорець перевірки email. |
| C-F20 | `AuthorSelection::caseSensitive` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Прапорець чутливості до регістру при фільтрації автора. |
| C-F21 | `DateRangeSelection::fromTime` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Нижня часова межа (`authoredAt`). |
| C-F22 | `DateRangeSelection::toTime` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Верхня часова межа (`authoredAt`). |
| C-F23 | `DateRangeSelection::inclusive` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Прапорець інклюзивного інтервалу. |

### Нетривіальні реалізації (19 фактичних)

Облік включає 4 методи `apply`, 5 методів `select`, 1 шаблонну функцію групування, 1 алгоритмічну функцію приведення до нижнього регістру (`toLowerCopy`) та 8 конструкторів із суворою валідацією інваріантів через `std::invalid_argument` (разом 19, до орієнтира 25 залишається 6):
1. `CommitRangeQuery::apply`
2. `FileHistoryQuery::apply`
3. `AuthorHistoryQuery::apply`
4. `MessageKeywordQuery::apply`
5. `LastNSelection::select`
6. `ExplicitSelection::select`
7. `PeriodicSelection::select`
8. `AuthorSelection::select`
9. `DateRangeSelection::select`
10. Шаблонна функція `groupRecords<Record, KeyFn>`
11. Алгоритмічна функція `toLowerCopy` (поелементне приведення символів до нижнього регістру через `std::transform` і `std::tolower`).
12-19. Конструктори з перевіркою інваріантів: `FileHistoryQuery`, `AuthorHistoryQuery`, `MessageKeywordQuery`, `LastNSelection`, `ExplicitSelection`, `PeriodicSelection`, `AuthorSelection`, `DateRangeSelection`.

## Інкапсуляція та ієрархії

Поля параметрів стратегій закриті через `private`, а доступ надано через методи `apply` та `select`.
Перша ієрархія — `HistoryQuery` (1 базовий інтерфейс + 4 похідні класи, разом 5 класів).
Друга ієрархія — `RevisionSelection` (1 базовий інтерфейс + 5 похідних класів, разом 6 класів).

## Відтворення і демонстрація

Журнал [перевірки коду для звітів ЛР №1](../lab1/lab1-verification.md) фіксує успішну збірку та проходження всіх цілей CTest, включаючи оновлений `history_test`, який перевіряє регістронезалежний пошук, часові діапазони (`authoredAt`), межі та валідацію інваріантів.

## Висновок і межі роботи

ООП-основа та класи модуля реалізовані у межах часткового внеску (11 типів, 23 поля, 19 нетривіальних реалізацій; до орієнтира 25 реалізацій залишається 6). Структурні вимоги та поліморфізм забезпечені у межах поточного PR.