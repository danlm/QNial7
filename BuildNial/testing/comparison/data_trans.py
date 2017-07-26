import numpy as np
a = np.random.rand(1024, 60000)
import time

n = 0
n_iter = 20

start_time = time.time()

while n < n_iter:
        for i, j in enumerate(a):
                m = j.mean()
                s = j.std()
                a[i] = a[i] - m
                a[i] = a[i]/s
        n = n + 1

print((time.time() - start_time)/n_iter)


