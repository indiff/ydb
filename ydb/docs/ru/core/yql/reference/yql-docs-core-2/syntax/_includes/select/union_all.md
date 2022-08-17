## UNION ALL {#union-all}

Конкатенация результатов нескольких `SELECT` (или подзапросов).

Поддерживаются два режима выполнения `UNION ALL` – по именам колонок (режим по умолчанию) и по позициям колонок (соответствует стандарту ANSI SQL и включается через соответствующую [PRAGMA](../../pragma.md#positionalunionall)).

В режиме "по именам" результирующая схема данных выводится по следующим правилам:

{% include [union all rules](union_all_rules.md) %}

Порядок выходных колонок в этом режиме выводится как наибольший общий префикс порядка входов, после чего следуют все остальные колонки в алфавитном порядке.
Если наибольший общий префикс пуст (в том числе и из-за отсутствия порядка на одном из входов), то порядок выхода не определен.

В режиме "по позициям" результирующая схема данных выводится по следующим правилам:
* число колонок во всех входах должно быть одинаковым
* порядок колонок во всех входах должен быть определен
* имена результирующих колонок совпадают с именами колонок первой таблицы
* тип результирующих колонок выводится как общий (наиболее широкий) тип из типов входных колонок стоящих на одинаковых позициях

Порядок выходных колонок в этом режиме совпадает с порядком колонок первого входа.

**Примеры**

``` yql
SELECT 1 AS x
UNION ALL
SELECT 2 AS y
UNION ALL
SELECT 3 AS z;
```

В результате выполнения данного запроса в режиме по-умолчанию будет сформирована выборка с тремя колонками x, y, и z. При включенной `PRAGMA PositionalUnionAll;` в выборке будет одна колонка x.


``` yql
PRAGMA PositionalUnionAll;

SELECT 1 AS x, 2 as y
UNION ALL
SELECT * FROM AS_TABLE([<|x:3, y:4|>]); -- ошибка: порядок колонок в AS_TABLE не определен
```