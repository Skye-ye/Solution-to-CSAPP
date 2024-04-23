# Solution to hw4

## Question 1

**A**：A + (i * S * T * 8) + (j * T * 8) +(k * 8)

**B**：因为

```
S * T = 65
T = 13
8 * R * S * T = 3640
```

所以

```C++
R = 7
S = 5
T = 13
```

## Question 2

```assembly
sum_col:
  leaq 1(,%rdi,4), %r8        # %r8 = n * 4 + 1
  leaq (%rdi,%rdi,2), %rax    # %rax = n * 3
  movq %rax, %rdi             # %rdi = n * 3
  testq %rax, %rax            # test n * 3
  jle .L4                     # n*3 <= 0, jump .L4
  salq $3, %r8                # %r8 = 8 * (n * 4 + 1)
  leaq (%rsi,%rdx,8), %rcx    # %rcx = A + 8 * j
  movl $0, %eax               # %rax = 0
  movl $0, %edx               # %rdx = 0
.L3:
  addq (%rcx), %rax           # %rax = *(A + j * 8)
  addq $1, %rdx               # %rax = %rax + 1
  addq %r8, %rcx              # #rcx = A + 8 * (4 * n + 1) + 8 * j
  cmpq %rdi, %rdx             # cmp %rax & n * 3
  jne .L3                     # if %rax < n * 3, loop
  rep
  ret
.L4:
  movl $0, %eax               # return 0
  ret
```

由`tests %rax %rax`这一行可知，如果3n小余等于零的话，直接结束程序返回0，那么就说明rax里的值就是`NR(N)`，则NR表达式应为`#define NR(N) 3 * N `

由`addq %r8, %rcx`可知，每一轮循环，`%rcx`的值会加上4n+1，由此可知每行有4n+1个元素，即NC表达式为`#define NC(N) 4 * N + !`
## Question 3

```assembly
# strB process(strA s)
# s in %rdi
process:
  movq %rdi, %rax      # 将rsp + 64存到rax中
  movq 24(%rsp), %rdx  # 把z的地址存到rdx中
  movq (%rdx), %rdx    # 把z存到rdx中
  movq 16(%rsp), %rcx  # 把y存到rcx中
  movq %rcx, (%rdi)    # 把y存到rdi对应的地址上
  movq 8(%rsp), %rcx   # 把x存到rcx中
  movq %rcx, 8(%rdi)   # 把x存到rdi + 8上
  movq %rdx, 16(%rdi)  # 把z存到rdi + 16上
  ret

# long eval(long x, long y, long z)
# x in %rdi, y in %rsi, z in %rdx
eval:
  subq $104, %rsp
  movq %rdx, 24(%rsp)  # 将z存到rsp偏移24字节的地址，即z存在比rsp高24个字节的栈帧上
  leaq 24(%rsp), %rax  # 将z的地址存到rax中
  movq %rdi, (%rsp)    # 将x存到rsp对应的地址，即栈顶
  movq %rsi, 8(%rsp)   # 将y存到rsp偏移8字节的地址
  movq %rax, 16(%rsp)  # 将z的地址存到rsp偏移16字节的地址
  leaq 64(%rsp), %rdi  # 将rsp偏移64字节的地址存到rdi中
  call process         # 调用process
  movq 72(%rsp), %rax  # rax += x
  addq 64(%rsp), %rax  # rax += y
  addq 80(%rsp), %rax  # rax += z
  addq $104, %rsp      # 重置rsp
  ret
```

**A**:

```assembly
104 +---------------+
    |               |
    |               |
    |               |
    |               |
    |               |
    |               |
    |               |
 64 +---------------+ <- %rdi
    |               |
    |               |
    |               |
    |               |
    |               |
    |               |
 32 +---------------+
    |       z       |
 24 +---------------+
    |       &z      |
 16 +---------------+
    |       y       |
  8 +---------------+
    |       x       |
  0 +---------------+ <- %rsp
```

**B**：eval函数通过%rdi，把%rsp + 64这一地址传入了process函数

**C**：通过%rsp这一地址并在其加上偏移量来获取结构体中各成员

**D**：通过%rsp + 64这一地址并在其加上偏移量

**E**：

```assembly
104 +---------------+
    |               |
    |               |
    |               |
    |               |
 88 +---------------+
    |       z       |
 80 +---------------+
    |       x       |
 72 +---------------+
    |       y       |
 64 +---------------+ <- %rdi (eval pass into process)
    |               |  \-- %rax (returned by process)
    |               |   
    |               |
    |               |
    |               |
    |               |
 32 +---------------+
    |       z       |
 24 +---------------+
    |       &z      |
 16 +---------------+
    |       y       |
  8 +---------------+
    |       x       |
  0 +---------------+ <- %rsp in eval
    |               |
 -8 +---------------+ <- %rsp in process  (这里的-8仅相对eval的地址而言)
```

**F**：调用者会事先预留好被调用者所需的空间并通过传入内存地址的形式告知被调用者数据存储位置，被调用者会接收该地址并处理存储数据，再将数据起始地址返回给调用者


## Question 4

**A**:

|  field  | offset |
| :-----: | :----: |
|  e1.p   |   0    |
|  e1.y   |   8    |
|  e2.x   |   0    |
| e2.next |   8    |

**B**：16

**C**：

```assembly
# void proc(union ele *up)
# up in %rdi
proc:
  movq 8(%rdi), %rax
  movq (%rax), %rdx   # 该行可知%rax是一个指针，得出上一行%rdi所指的是e2
                      # 那么8(%rdi)便是up->e2.next
  movq (%rdx), %rdx   # 该行可知%rdx也是一个指针，所以%rdx对应e1.p
  subq 8(%rax), %rdx  # 该行可知8(%rax)是一个数，因此为e2.y
  movq %rdx, (%rdi)   # 因为rdx是个数，所以(%rdi)也是个数，即e1.x
  ret
```

答案为

```C++
void proc(union ele *up) {
  up->e1.x = *(up->e2.next->e1.p) - (up->e2.next->e1.y);
}
```

