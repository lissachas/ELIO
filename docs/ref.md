Compilation:
rm -rf build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++

rm -rf build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm
cmake --build build
./build/app

git add .
git commit -m "Parser fully done"
git push

git add .
git commit -m "refactor: migrate to C++23 modules"
git push origin modules

./build/elio tests/01.el -o out/01.ll
./build/elio tests/02.el -o out/02.ll
./build/elio tests/03.el -o out/03.ll
./build/elio tests/04.el -o out/04.ll
./build/elio tests/05.el -o out/05.ll
./build/elio tests/06.el -o out/06.ll
./build/elio tests/07.el -o out/07.ll
./build/elio tests/08.el -o out/08.ll
./build/elio tests/09.el -o out/09.ll
./build/elio tests/10.el -o out/10.ll
./build/elio tests/11.el -o out/11.ll
./build/elio tests/12.el -o out/12.ll
./build/elio tests/13.el -o out/13.ll

./build/elio tests/01.el -o out/01.ll --dump-tokens
./build/elio tests/01.el -o out/01.ll --dump-ast
./build/elio tests/01.el -o out/01.ll --opt


llc -filetype=obj out/13.ll -o out/13.o
clang out/13.o -o out/13
./out/13

2. Reference counting / ARC

У каждого объекта есть счётчик ссылок:

создали новую ссылку → +1
убрали ссылку → -1
стало 0 → освобождаем
Для автора языка:

Это часто самый разумный компромисс.

Почему проще, чем ownership:
не нужен borrow checker
не нужно доказывать lifetime на этапе компиляции
логика локальная и понятная
можно вставлять инкременты/декременты прямо в IR или AST lowerings
Что надо реализовать:
вставку retain/release
освобождение при refcount == 0
weak references, если хочешь избежать части проблем
возможно cycle detection или запрет циклов
Главная проблема:

циклы ссылок:

A -> B
B -> A
счётчики не падают до нуля
память течёт
Вердикт:

Если тебе нужен язык, который реально можно поднять руками без написания диссертации про lifetime inference, то
ARC / reference counting — probably самый практичный старт.

3. Простой tracing GC

Это уже полноценный garbage collector.

Самый простой вариант:

mark-and-sweep
иногда stop-the-world
без поколений, без компактации, без сложной магии, которой люди потом гордятся на конференциях
Для автора языка:

Это проще, чем ownership, но сложнее, чем reference counting.

Почему не так страшно:
компилятор проще, чем с ownership
часть сложности уходит в runtime
программисту удобно
Что реально надо:
heap allocator
список/таблица объектов
root scanning
mark phase
sweep phase
Где начинается боль:
нужно точно знать, где ссылки, а где не ссылки
нужно проектировать layout объектов
нужно решать, как находить корни:
стек
глобальные переменные
регистры, если пойдёшь в нативный код
появляются паузы и менее детерминированное поведение
Вердикт:

Простой GC реализуем, если ты готов написать runtime.
Но если ты хочешь “предсказуемость” как принцип языка, GC уже слегка пахнет компромиссом.

4. Ownership + borrow checking

Вот это уже взрослая боль.

Для автора языка:

Это самое сложное из перечисленного.

Потому что тебе надо:

отслеживать владельца каждого значения
отслеживать перемещения (move)
анализировать заимствования (borrow)
запрещать конфликтующие доступы
считать lifetimes
объяснять ошибки так, чтобы пользователь не проклял твой род
Почему Rust настолько впечатляет:

потому что это реально трудно сделать хорошо.

Вердикт:

Не начинай с этого, если ты делаешь язык один или маленькой командой, если только у тебя нет странного желания провести год в борьбе с alias analysis и депрессией компилятора.