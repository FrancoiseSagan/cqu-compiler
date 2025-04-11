# cqu-compiler

重庆大学 编译原理

### homework 1

这里需要识别的符号非常简单，只有加减乘除和括号，因此逻辑也非常简单，遇到数字和字母就匹配整个数字，否则匹配运算符即可

### homework 2

建议直接使用递归下降解析，否则真的比较麻烦，连错在哪都不好找

这里需要识别四种进制数字（注意这里的八进制不是0o）：

- 二进制 0b1
- 八进制 01
- 十进制 1
- 十六进制 0x1

在作业2中，只需要查看下一个token就可以知道当前如何进行解析，因此是 **LL(1)** 文法，使用递归下降解析即可喵

**递归下降**解析有两个重要的操作：

- **peek/lookahead**：查看下一个token，判断接下来如何进行解析
- **advance/consume**：token index前进

接下来只需要按照作业中给出的产生式进行匹配即可：

```
Exp -> AddExp
Number -> IntConst | floatConst
PrimaryExp -> '(' Exp ')' | Number
UnaryExp -> PrimaryExp | UnaryOp UnaryExp
UnaryOp -> '+' | '-'**
MulExp -> UnaryExp { ('\*' | '/') UnaryExp }
AddExp -> MulExp { ('+' | '-') MulExp }
```

减和除没有专门的node，我是用value判断的