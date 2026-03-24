Prompts:

目前代码中 (row, column) -> row * stride + column * 4， 0-255 uint8_t 的 WinRT API native 布局（以下称为 WinRT 布局）不是连续的，但这对于通道颜色值的批量计算而言并不 native（我们把符合下列条件的布局称为连续布局）：
1. 其基于 0-255 范围，而通道颜色值的计算是从 float 到 float； 
2. 其不能一次性直接读取或写入 std::array<float, 4> 视图；
3. 循环间隔不为1，且底层数组访问可能跳变，不利于 SIMD、并行化等优化；
但是为了和 WinRT API 交互，需要：
1. 能够从 WinRT 布局的数组（有迭代器，你不需要担心这一点）复制入和复制出，因此至少需要一个 WinRT 布局视角的迭代器，从而可以使用 std::copy。

请编写一个类，其底层实现是连续布局：std::vector<std::array<float>, 4>（请顺带把本项目中的 double 都替换为 float），其内部亦储存有足以从 (row, column) 推导 WinRT 布局下标的 stride 等，以及将 0-255 范围和 0-1 float 转换所需的静态函数。
底层 vector 的大小一开始就已经确定，且一旦设定就不能更改，所以你可以安全地假设到底层 vector 内部的指针、引用和迭代器永远不会失效。

你需要实现：
1. 构造函数，其参数至少包含 stride 及逻辑大小，它们会被用来初始化 const private 成员，这些数值一旦设定就不能改变；
2. private vector 的大小是由 row 和 column 决定的，和 stride 无关；
3. 对于连续视图，支持随机访问，因此你只需要暴露底层数组的 .data() ；
4. WinRT 布局视角迭代器是一个 ForwardIterator，支持 ++ 以及读写。WinRT 布局视角迭代器内部，除了 WinRT 布局下标，也包含了 .data() 的指针、stride 等一切使其可以自行计算 WinRT 布局下标到连续布局下标的对应关系的 private 成员变量，从而能够自行正确计算 WinRT 布局下标对应的连续布局读写位置。如果当前的 WinRT 布局下标没有对应的连续布局下标，那么对其的读取返回一个假值（如 -1），对其的写入是一个 no-op，两者均不会抛出任何异常。对其余位置的读写，将会涉及到 0-1范围 float 颜色分量与 0-255 范围 uint8_t 颜色分量的相互转换。

实现后，请将 MainWindow.xaml.cpp 619、621 行的循环改写为对于连续数组的连续访问。

请在单独的文件内编写。

---

- 看起来 TryResolveMapping 设为成员函数要更简洁
- 这两个循环，目前只需要遍历.data()的所有元素就行了吧（你可以添加求连续布局长度的方法）