# Фактичний внесок C: Git та історія

## Мета і виконана частина

Звіт описує код гілки PR `feature/lab1_C_completion` (на базі `origin/main` після злиття `feature/lab1_C_requirements`). Виконана частина C — дві ієрархії запитів і вибору комітів, їхні операції над переданими записами, шаблонне групування та допоміжні аналітичні функції. Код міститься у [`src/history/types.hpp`](../../src/history/types.hpp), а клієнтські приклади й assertions — у [`tests/history_test.cpp`](../../tests/history_test.cpp).

Облік зроблено за повним diff від E0, а не лише за `git blame`. Початковий `HistoryRequest`, `RepositorySpec`, `CommitRecord`, `ChangeRecord`, `HistoryResult` та спільні контракти походять з E0 (`c8ed63c17be2dbaaa0dc2ccdeccc6c6106e1573f`, автор A) і не зараховуються C. `Oid` є alias і також не рахується типом.

## Відповідність вимогам

| Вимога ЛР №1 | Фактично в C | Статус |
|---|---:|---|
| Класи або подібні типи | 13 | Виконано (очікувалося >= 9) |
| Поля | 29 | Виконано (очікувалося >= 15) |
| Нетривіальні методи або функції | 27 | Виконано (орієнтир 25) |
| Ієрархії успадкування | 2 ієрархії (6 та 7 класів із базовими) | Виконано |
| Незалежні поліморфні випадки | 2 динамічні через базові типи та 1 статичний шаблонний | Виконано |

### Типи

| ID | Тип C | Файл і символ | Стан та доказ |
|---|---|---|---|
| C-T01 | `HistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `HistoryQuery` | Реалізовано як абстрактний базовий клас із віртуальним `apply`. |
| C-T02 | `CommitRangeQuery` | [`types.hpp`](../../src/history/types.hpp), `CommitRangeQuery` | Реалізовано відбір меж у списку комітів із валідацією. |
| C-T03 | `FileHistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `FileHistoryQuery` | Реалізовано фільтр за старим або новим шляхом (з підтримкою `strictMatch`). |
| C-T04 | `AuthorHistoryQuery` | [`types.hpp`](../../src/history/types.hpp), `AuthorHistoryQuery` | Реалізовано фільтрацію за автором/email із підтримкою `caseSensitive`. |
| C-T05 | `MessageKeywordQuery` | [`types.hpp`](../../src/history/types.hpp), `MessageKeywordQuery` | Реалізовано фільтрацію за ключовим словом із підтримкою `matchCase`. |
| C-T06 | `MergeCommitQuery` | [`types.hpp`](../../src/history/types.hpp), `MergeCommitQuery` | Реалізовано фільтрацію злиттів/не-злиттів із перевіркою `minParents`. |
| C-T07 | `RevisionSelection` | [`types.hpp`](../../src/history/types.hpp), `RevisionSelection` | Реалізовано як абстрактний базовий клас із віртуальним `select`. |
| C-T08 | `LastNSelection` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Реалізовано вибір перших N записів (із обробкою `allowOverflow`). |
| C-T09 | `ExplicitSelection` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Реалізовано вибір заданих OID із дедуплікацією та перевіркою `strictExistence`. |
| C-T10 | `PeriodicSelection` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Реалізовано вибірку з кроком та зміщенням; нульовий крок відхиляється. |
| C-T11 | `AuthorSelection` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Реалізовано вибірку ревізій за автором з урахуванням `caseSensitive`. |
| C-T12 | `DateRangeSelection` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Реалізовано вибірку комітів за часовим діапазоном `authoredAt` (`from <= to`). |
| C-T13 | `LinearHistorySelection` | [`types.hpp`](../../src/history/types.hpp), `LinearHistorySelection` | Реалізовано обхід лінійного ланцюга комітів за `firstParent` через BFS. |

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
| C-F12 | `MergeCommitQuery::onlyMerges` | [`types.hpp`](../../src/history/types.hpp), `MergeCommitQuery` | Прапорець фільтрації лише merge-комітів. |
| C-F13 | `MergeCommitQuery::minParents` | [`types.hpp`](../../src/history/types.hpp), `MergeCommitQuery` | Мінімальна кількість батьківських OID для merge-коміта. |
| C-F14 | `LastNSelection::count` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Кількість вибраних комітів. |
| C-F15 | `LastNSelection::allowOverflow` | [`types.hpp`](../../src/history/types.hpp), `LastNSelection` | Прапорець дозволу переповнення. |
| C-F16 | `ExplicitSelection::targetOids` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Список цільових OID. |
| C-F17 | `ExplicitSelection::strictExistence` | [`types.hpp`](../../src/history/types.hpp), `ExplicitSelection` | Сувора перевірка наявності OID. |
| C-F18 | `PeriodicSelection::step` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Крок вибірки. |
| C-F19 | `PeriodicSelection::offset` | [`types.hpp`](../../src/history/types.hpp), `PeriodicSelection` | Початкове зміщення вибірки. |
| C-F20 | `AuthorSelection::authorName` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Ім'я автора для вибірки. |
| C-F21 | `AuthorSelection::matchEmailToo` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Прапорець перевірки email. |
| C-F22 | `AuthorSelection::caseSensitive` | [`types.hpp`](../../src/history/types.hpp), `AuthorSelection` | Прапорець чутливості до регістру при фільтрації автора. |
| C-F23 | `DateRangeSelection::fromTime` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Нижня часова межа (`authoredAt`). |
| C-F24 | `DateRangeSelection::toTime` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Верхня часова межа (`authoredAt`). |
| C-F25 | `DateRangeSelection::inclusive` | [`types.hpp`](../../src/history/types.hpp), `DateRangeSelection` | Прапорець інклюзивного інтервалу. |
| C-F26 | `LinearHistorySelection::startOid` | [`types.hpp`](../../src/history/types.hpp), `LinearHistorySelection` | Початковий OID для обходу. |
| C-F27 | `LinearHistorySelection::firstParentOnly` | [`types.hpp`](../../src/history/types.hpp), `LinearHistorySelection` | Прапорець обходу лише першого батька. |
| C-F28 | `CodeChurn::totalAddedLines` | [`types.hpp`](../../src/history/types.hpp), `CodeChurn` | Загальна кількість доданих рядків. |
| C-F29 | `CodeChurn::totalDeletedLines` | [`types.hpp`](../../src/history/types.hpp), `CodeChurn` | Загальна кількість видалених рядків. |

### Нетривіальні реалізації (27 фактичних)

Облік включає 5 методів `apply`, 6 методів `select`, 1 шаблонну функцію групування, 1 алгоритмічну функцію `toLowerCopy`, 4 аналітичні вільні функції, та 10 конструкторів із суворою валідацією інваріантів через `std::invalid_argument`:

**Методи apply (HistoryQuery-ієрархія):**
1. `CommitRangeQuery::apply` — перевіряє межі, порядок, повертає зріз
2. `FileHistoryQuery::apply` — фільтрує за шляхом (strict/loose match)
3. `AuthorHistoryQuery::apply` — фільтрує за автором з урахуванням регістру
4. `MessageKeywordQuery::apply` — пошук ключового слова в повідомленні
5. `MergeCommitQuery::apply` — фільтр merge/non-merge комітів за кількістю батьків

**Методи select (RevisionSelection-ієрархія):**
6. `LastNSelection::select` — вибір N перших комітів
7. `ExplicitSelection::select` — вибір за OID із дедуплікацією та збереженням порядку
8. `PeriodicSelection::select` — вибірка з кроком, захист від size_t overflow
9. `AuthorSelection::select` — фільтрація за іменем/email автора
10. `DateRangeSelection::select` — фільтрація за `authoredAt` у діапазоні
11. `LinearHistorySelection::select` — BFS-обхід першого батька від стартового OID

**Аналітичні вільні функції:**
12. `computeCodeChurn` — агрегація доданих/видалених рядків та бінарних файлів
13. `isAncestor` — BFS-перевірка предківства в DAG комітів
14. `filterChangesByStatus` — фільтрація `ChangeRecord` за `ChangeStatus`
15. `sortCommitsChronologically` — стабільне сортування комітів за `authoredAt`

**Узагальнений шаблонний алгоритм:**
16. Шаблонна функція `groupRecords<Record, KeyFn>`

**Алгоритмічна утиліта:**
17. `toLowerCopy` — поелементне приведення рядка до нижнього регістру

**Конструктори з перевіркою інваріантів (10):**
18. `FileHistoryQuery` — порожній шлях
19. `AuthorHistoryQuery` — порожній автор
20. `MessageKeywordQuery` — порожнє ключове слово
21. `MergeCommitQuery` — `onlyMerges=true` і `minParents<2`
22. `LastNSelection` — count=0
23. `ExplicitSelection` — порожній список OID
24. `PeriodicSelection` — step=0
25. `AuthorSelection` — порожнє ім'я
26. `DateRangeSelection` — `from > to`
27. `LinearHistorySelection` — порожній startOid

## Інкапсуляція та ієрархії

Поля параметрів стратегій закриті через `private`, а доступ надано через методи `apply` та `select`.
Перша ієрархія — `HistoryQuery` (1 базовий інтерфейс + 5 похідних класів, разом 6 класів).
Друга ієрархія — `RevisionSelection` (1 базовий інтерфейс + 6 похідних класів, разом 7 класів).

## Відтворення і демонстрація

Журнал [перевірки коду для звітів ЛР №1](../lab1/lab1-verification.md) фіксує успішну збірку та проходження всіх цілей CTest, включаючи `history_test`, який перевіряє всі нові класи та функції.

## Висновок і межі роботи

ООП-основа та класи модуля повністю реалізовані (13 типів, 29 полів, 27 нетривіальних реалізацій; орієнтир 25 реалізацій виконано). Структурні вимоги, поліморфізм та аналітичні утиліти забезпечені у повному обсязі.
