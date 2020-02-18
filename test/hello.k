extern kout(in)
def inlineMul (x) x*2
def testAdd (x y) x+inlineMul(y)
def main() kout(testAdd(1 2))
