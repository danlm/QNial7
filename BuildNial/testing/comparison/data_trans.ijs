NB. Numeric tests to compute the number of
NB. standard deviations from the mean for each element
NB. of a 1024x60000 real matrix

NB. Create a test matrix

testm =: ?"0 ( 1024 60000 {. 0)

NB. Monadic vector function to compute the number of
NB. standard deviations from the mean for each element
NB. of the vector

transv =: monad define
  avg =: (+/ y) % #y
  mean_offset =: y - avg
  std =: 2 %: ((+/ (mean_offset*mean_offset)) % #y)
  mean_offset % std
)


NB. Test is run by loading this script and computing
NB. the average time over 10 iterations as follows -
NB. 
NB. 10 (6!:2) 'transv"1 testm'
NB.