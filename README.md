# Scheme Interpreter

This is my implementation for an interpreter of [Scheme](https://en.wikipedia.org/wiki/Scheme_(programming_language)). It was written as a project for the advanced C++ course in my university. It doesn't include all of the features of the language, but it includes the main ones: integer arithmetic, variables and their definition, lists, functions and lambdas. Possible use cases can be seen in the `tests` folder.

Short description of the interpretation algorithm:
1) Parse input sequence into tokens
2) Construct an abstract syntax tree from the constructed sequence
3) Evaluate the tree, which may include variable manipulation or running user-implemented functions that were declared in the past
4) Run mark-and-sweep garbage collection, since object dependancies can be cyclical and all of the objects are created on the heap
