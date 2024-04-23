# Solution to hw02

## Q1

```C
(y & ~0xFF) | (x & 0xFF)
```

## Q2

```C
unsigned replace_byte(unsigned x, int i, unsigned char b) {
    unsigned mask = ~(0xFF << (i * 8));
    unsigned cleared = x & mask;
    unsigned replace = b << (i * 8);
    return cleared | replace;
}
```



## Q3

### A

`int beyond_msb = 1 << 32;`  这一行有问题，其左移大小超过了字符比特长度减一（w - 1）。因此会引起未定义行为，使编译器发出警告。

### B

```c
int int_size_is_32() {
    int set_msb = 1 << 31;
    int beyond_msb = set_msb << 1; // 每次运算不超过字长大小
    return set_msb && !beyond_msb;
}
```

### C

```c
int int_size_is_32() {
    unsigned int set_msb = 1u << 15;
    set_msb <<= 15; // 为在int为16bit的机器上运行，每次左移不超过15
    set_msb <<= 1;
    unsigned int beyond_msb = set_msb << 1;
    return set_msb && !beyond_msb;
}
```



## Q4

### A

这段代码错在，当获取的`unsigned int`字节的最高位上为1的时候，转换后的`int`应为服输，但这一算法得到的结果却是正数。

### B

```c
int xbyte(packed_t word, int bytenum) {
  // 先将word左移，使word最高位上是所要提取byte的最高位
  int shifted = (int)(word << ((3 - bytenum) << 3));
  // 再执行右移运算，高位自动填入1或0，实现正负
  return shifted >> 24; 
}
```



## Q5

**A:** 该表达不恒为1， 当 `x` 取 `INT_MAX` ,  `y` 取 `INT_MIN` 时，该表达式的值为0， 因为`INT_MIN`取负数时发生了溢出。

**B:** 该表达式恒为1，`(x + y) << 4` 等价于 `16 * (x + y)` ，因此等式两边运算后相等。同时测试了32bit整数所能容纳的最小值和最大值，结果也相等。因为虽然等式两边都溢出，但两边的溢出大小和方式相同，因此最后溢出剩余结果也相同。

**C:** 该表达式恒为1，`~x` 表示对 `x` 按位取反，得到的结果是 `-x + 1` ，因此等式两边相等。同时按位取反并不会造成任何溢出。

**D:** 该表达式恒为1。分析四种情况：当 `x` 和 `y` 都是正数时，转换为 `unsigned int` 后不会有区别，表达式相等。当 `x` 为负， `y` 为正时， `ux` 会变成一个很大的数，`y - x` 被转换为 `unsigned int` 时是一个较小的正数，但取负后会变为很大的正数，剩下两种情况同理。

**E:** 该表达式恒为1。当x能被4整除，即最低两位为0的时候，右移两位再左移两位并不会改变造成数字变小。相反，最低两位有1时，右移时会丢失。

