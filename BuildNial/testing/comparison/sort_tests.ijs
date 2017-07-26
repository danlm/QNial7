
NB. niter test_sort_int n
NB. averages the performance of /:~ over
NB. niter iterations selecting n random integers from
NB. [0..n)
 
test_sort_int =: dyad define 
  cn =: 0
  durn =: 0.0
  while. cn < x do.
    d =: ? (y + (y {. 0))
    durn =: durn + (5 (6!:2) '(/:~) d')
    cn =: cn + 1
  end. 
  durn%x
)


NB. niter test_sort_real n
NB. averages the performance of /:~ over
NB. niter iterations selecting n random reals from
NB. [0.0 .. 1.0)
 
test_sort_real =: dyad define 
  cn =: 0
  durn =: 0.0
  while. cn < x do.
    d =: ? (y {. 0)
    durn =: durn + (5 (6!:2) '(/:~) d')
    cn =: cn + 1
  end. 
  durn%x
)

