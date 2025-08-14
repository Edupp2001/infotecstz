

## Сборка

```bash
mkdir build && cd build
cmake ..
make
```

## Запуск app

```
./app/LoggerApp <log_file> <default_level>
```

- `<log_file>` — путь к файлу журнала (например, `log.txt`)
- `<default_level>` — уровень логирования по умолчанию: `error`, `warning`, `info`

### Пример:
```
./app/LoggerApp log.txt info
```

## Ввод сообщений

Вводите сообщения в консоль. Можно указать уровень:
```
error: критическая ошибка
warning: предупреждение
информационное сообщение (будет записано с уровнем по умолчанию)
```

Для завершения введите:
```
exit
```

Для записи в сокет
```
./app/LoggerApp <log_file> <default_level> <ip> <port>
```
## Запуск статистики
```
./app_stats/LoggerStatsApp <port> <N> <T>
```
- `<N>` - вывод после приема  N  сообщений
- `<T>` - вывод после Т секунд

## Требования
- C++17
- CMake >= 3.10
- gcc/g++
# infotecstz
# infotecstz
# infotecstz
# infotecstz
