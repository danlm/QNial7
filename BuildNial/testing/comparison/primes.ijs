NB. Test the peformance of J in computing 
NB. primes in a simple way

is_prime =: monad define
  (y > 1) *. (*./ (0 < ((2 }. i. (1 + <. 2 %: y)) | y)))
)

isp_time =: dyad define
  d =: 1 + i. y 
  x (6!:2) '(is_prime"0) d'

isp_list =: monad define
  d =: 1 + i. y 
  (is_prime"0) d
)

