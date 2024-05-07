# Solution to hw05

## 7.6

| Symbol | swap.o.symtab entry? | Synbol type | Module where defined | Section |
| ------ | :------------------: | :---------: | :------------------: | :-----: |
| buf    |         YES          |   extern    |         m.o          |  .data  |
| bufp0  |         YES          |   global    |        swap.o        |  .data  |
| bufp1  |         YES          |    local    |        swap.o        |  .bss   |
| swap   |         YES          |   global    |        swap.o        |  .text  |
| temp   |          NO          |     --      |          --          |   --    |
| incr   |         YES          |    local    |        swap.o        |  .text  |
| count  |         YES          |    local    |        swap.o        |  .bss   |

## 7.7

```C
/* bar5.c */
static double x;

void f() {
  x = -0.0;
}
```

## 7.8

**A**: (a) main.1 (b) main.2

**B**: (a) UNKNOWN (b) UNKNOWN

**C**: (a) ERROR (b) ERROR

## 7.9

运行下面指令编译并反汇编，其中-fcommon是为了防止新版gcc对未初始化的全局变量报错（本例中的char main），-fcf-protection=none是为了取消CET（用来防止JOP和ROP注入攻击），CET会在部分函数前加上endbr64对跳转进行检测。

```bash
gcc -Og foo6.c bar6.c -o main6 -fcommon -fcf-protection=none
objdump -d main6
```

可以得到：

```assembly
0000000000001139 <main>:
    1139:       48 83 ec 08             sub    $0x8,%rsp
    113d:       e8 0a 00 00 00          call   114c <p2>
    1142:       b8 00 00 00 00          mov    $0x0,%eax
    1147:       48 83 c4 08             add    $0x8,%rsp
    114b:       c3                    ret
```

可以看到main函数的第一个字节就是0x48，那么被narrow down到char类型后就变成0x48。

## 7.10

```bash
gcc p.o libx.a
```

```bash
gcc p.o libx.a liby.a libx.a
```

```bash
gcc p.o libx.a liby.a libx.a libz.a
```

## 7.11

为.bss预留的空间

## 7.12

**A**:

ADDR(s) = ADDR(.text) = 0x4004e0

ADDR(r.symbol) = ADDR(swap) = 0x4004f8

refaddr = ADDR(s) + r.offset = 0x4004ea

*refptr = (unsigned) (ADDR(r.symbol) + r.addend - refaddr) = 0xa

**B**:

ADDR(s) = ADDR(.text) = 0x4004d0

ADDR(r.symbol) = ADDR(swap) = 0x400500

refaddr = ADDR(s) + r.offset = 0x4004da

*refptr = (unsigned) (ADDR(r.symbol) + r.addend - refaddr) = 0x22
