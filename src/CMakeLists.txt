cmake_minimum_required(VERSION 3.10)

# Имя проекта
project(IPC_MMAP_SIGNALS C)

# Стандарт языка
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Опции для компилятора (полезно)
add_compile_options(-Wall -Wextra -pedantic)

# Добавляем две отдельные программы
add_executable(parent parent.c)
add_executable(child  child.c)

# На Linux обычно нужно линковать rt (если используете clock_gettime)
# Но mmap/signal/kill работают без дополнительных библиотек
