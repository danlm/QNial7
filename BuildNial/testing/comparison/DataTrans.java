

// Create an array of vectors of random numbers
// For each vector compute its mean and standard
// deviation and adjust the entries of the vector
// to be number of standard deviations from the mean


class DataTrans {

    static final int nentries = 60000;
    static final int narrays  = 1024;
    static final int n_iter = 20;

    // The initial array of arrays 
    static double[][] a = null;

    // Hold for result
    static double[][] res = new double[narrays][];

    private static  double random_val() {
        return Math.random();
    }

    // Initialise the program
    private static  void setup() {
        a = new double[narrays][nentries];
        for (int i = 0; i < narrays; i++) {
            for (int j = 0; j < nentries; j++)
                a[i][j] = random_val();
        }
        return;
    }

    private static  double[] one_row(double[] inp) {
        double avg = 0.0, std = 0.0;
        double[] d = new double[nentries];
        for (int j = 0; j < nentries; j++) 
            avg += (d[j] = inp[j]); 
        avg /= nentries;
        for (int j = 0; j < nentries; j++) {
            double ev = d[j] - avg;
            std += ev*ev;
            d[j] = ev;
        }
        std = Math.sqrt(std/nentries);
        for (int j = 0; j < nentries; j++)
            d[j] /= std;

        return d;
    }


    public static void main(String[] args) {

        setup();

	// Get Jitted so no complaints
	res[0] = one_row(a[0]);
        
        long startTime = System.nanoTime();
        for (int i = 0; i < n_iter; i++) 
            for (int j = 1; j < narrays; j++) 
                res[j] = one_row(a[j]);
        long endTime = System.nanoTime();
        
        System.out.println("Average time: " + ((endTime - startTime)/(1.0*n_iter)));
        
    }
}

