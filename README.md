# External merge sort in C++

## Параметры запуска:
```sh
./excutable table_name sorting_column sorting_type [memory_size] [K]
```

- `table_name`  - имя таблицы в формате csv.
- `sorting_column` - номер сортируемого столбца. 
    - Столбцы нумеруются с 0.
- `sorting_type` - тип сортируемого столбца.
    - 3 возможных значения: int, float или string.
- `memory_size` - размер доступной оперативной памяти в мегабайтах.
    - По умолчанию 500 Мб.
- `K` - число от 1 до 10, т.ч. доля оперативной памяти, заполненная сливаемыми блоками, не превосходит `K/10`. 
    - По умолчанию 4.

## Пример использования:
![Usage example](https://github.com/zdikov/external-merge-sort/raw/master/images/example.png)
