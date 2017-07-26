import math
import time


def isprime1(n):
  return (0==len([x for x in range(2, 1+int(math.sqrt(n))) if (n%x)==0]))


def isprime(m):
  res = [0==len([x for x in range(2, 1+int(math.sqrt(n))) if (n%x)==0]) for n in range(2,m)]
  return res

t1 = time.time()
s = [x for x in range(2,1000000) if isprime1(x)]
t2 = time.time()
print 'Duration',(t2-t1)
