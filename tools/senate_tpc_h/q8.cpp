namespace senate_tpc_h_q8 {

#define ZERO Integer(width, 0, PUBLIC)
#define TEN Integer(width, 10, PUBLIC)

    typedef struct _irow_13 {
        Integer partkey;
        Integer volume;
        Integer suppkey;
        Integer orderkey;
    } irow_13;

    typedef struct _orow_13 {
        Integer volume;
        Integer suppkey;
        Integer orderkey;
    } orow_13;

    typedef struct _irow_123 {
        Integer volume;
        Integer suppkey;
        Integer orderkey;
        Integer nationkey;
    } irow_123;

    typedef struct _orow_123 {
        Integer volume;
        Integer orderkey;
        Integer nationkey;
    } orow_123;

    typedef struct _irow_45 {
        Integer orderkey;
        Integer orderdate;
        Integer custkey;
        Integer nationkey;
    } irow_45;

    typedef struct _orow_45 {
        Integer orderkey;
        Integer orderdate;
        Integer nationkey;
    } orow_45;

    typedef struct _irow_67 {
        Integer nationkey;
        Integer name;
        Integer regionkey;
    } irow_67;

    typedef struct _orow_67 {
        Integer nationkey;
        Integer name;
    } orow_67;

    typedef struct _irow_4567 {
        Integer orderkey;
        Integer orderdate;
        Integer nationkey;
        Integer name;
    } irow_4567;

    typedef struct _orow_4567 {
        Integer orderkey;
        Integer orderdate;
        Integer nationkey;
        Integer name;
    } orow_4567;

    typedef struct _irow_1234567 {
        Integer volume;
        Integer orderkey;
        Integer orderdate;
        Integer nationkey;
        Integer name;
    } irow_1234567;

    typedef struct _orow_1234567 {
        Integer volume;
        Integer orderdate;
        Integer name;
    } orow_1234567;

    Integer integer_bit_and(Integer A, Bit B) {
        A[0] = A[0] & B;
        return A;
    }

    void cmp_swap(std::vector<irow_13>& row, int i, int j) {
        Bit to_swap = (row[i].partkey > row[j].partkey);
        swap(to_swap, row[i].partkey, row[j].partkey);
        swap(to_swap, row[i].volume, row[j].volume);
        swap(to_swap, row[i].suppkey, row[j].suppkey);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
    }

    void cmp_swap(std::vector<orow_13>& row, int i, int j) {
        Bit to_swap = (row[i].suppkey > row[j].suppkey);
        swap(to_swap, row[i].volume, row[j].volume);
        swap(to_swap, row[i].suppkey, row[j].suppkey);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
    }

    void cmp_swap(std::vector<irow_45>& row, int i, int j) {
        Bit to_swap = (row[i].custkey > row[j].custkey);
        swap(to_swap, row[i].custkey, row[j].custkey);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].orderdate, row[j].orderdate);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
    }

    void cmp_swap(std::vector<orow_45>& row, int i, int j) {
        Bit to_swap = (row[i].nationkey > row[j].nationkey);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].orderdate, row[j].orderdate);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
    }

    void cmp_swap(std::vector<irow_67>& row, int i, int j) {
        Bit to_swap = (row[i].regionkey > row[j].regionkey);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
        swap(to_swap, row[i].name, row[j].name);
        swap(to_swap, row[i].regionkey, row[j].regionkey);
    }

    void cmp_swap(std::vector<orow_67>& row, int i, int j) {
        Bit to_swap = (row[i].nationkey > row[j].nationkey);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
        swap(to_swap, row[i].name, row[j].name);
    }

    void cmp_swap(std::vector<irow_4567>& row, int i, int j) {
        Bit to_swap = (row[i].nationkey > row[j].nationkey);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
        swap(to_swap, row[i].name, row[j].name);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].orderdate, row[j].orderdate);
    }

    void cmp_swap(std::vector<orow_4567>& row, int i, int j) {
        Bit to_swap1 = (row[i].nationkey > row[j].nationkey);
        Bit to_swap2 = (row[i].orderkey > row[j].orderkey);
        Bit to_swap = to_swap1 & to_swap2;
        swap(to_swap, row[i].nationkey, row[j].nationkey);
        swap(to_swap, row[i].name, row[j].name);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].orderdate, row[j].orderdate);
    }

    void cmp_swap(std::vector<irow_123>& row, int i, int j) {
        Bit to_swap = (row[i].suppkey > row[j].suppkey);
        swap(to_swap, row[i].suppkey, row[j].suppkey);
        swap(to_swap, row[i].volume, row[j].volume);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
    }

    void cmp_swap(std::vector<orow_123>& row, int i, int j) {
        Bit to_swap1 = (row[i].nationkey > row[j].nationkey);
        Bit to_swap2 = (row[i].orderkey > row[j].orderkey);
        Bit to_swap = to_swap1 & to_swap2;
        swap(to_swap, row[i].volume, row[j].volume);
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
    }

    void cmp_swap(std::vector<irow_1234567>& row, int i, int j) {
        Bit to_swap1 = (row[i].nationkey > row[j].nationkey);
        Bit to_swap2 = (row[i].orderkey > row[j].orderkey);
        Bit to_swap = to_swap1 & to_swap2;
        swap(to_swap, row[i].orderkey, row[j].orderkey);
        swap(to_swap, row[i].nationkey, row[j].nationkey);
        swap(to_swap, row[i].name, row[j].name);
        swap(to_swap, row[i].orderdate, row[j].orderdate);
        swap(to_swap, row[i].volume, row[j].volume);
    }

    void cmp_swap(std::vector<orow_1234567>& row, int i, int j) {
        Bit to_swap = (row[i].orderdate > row[j].orderdate);
        swap(to_swap, row[i].name, row[j].name);
        swap(to_swap, row[i].orderdate, row[j].orderdate);
        swap(to_swap, row[i].volume, row[j].volume);
    }

    template <typename T>
    void keyMerge(std::vector<T>& row, int lo, int n, bool acc = true) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++) {
                cmp_swap(row, i, i + m);
            }
            if (acc) {
                keyMerge(row, lo, n - m, acc);
                keyMerge(row, lo + n - m, m, acc);
            } else {
                keyMerge(row, lo, m, acc);
                keyMerge(row, lo + m, n - m, acc);
            }
        }
    }

    template <typename T>
    void keySort(std::vector<T>& row, int lo, int n, bool acc = true) {
        if (n > 1) {
            int m = n / 2;
            keySort(row, lo, m, true);
            keySort(row, lo + m, n - m, false);
            keyMerge(row, lo, n, acc);
        }
    }

    inline irow_13 select_3(irow_13 A, irow_13 B, irow_13 C) {
        Bit eq1 = B.partkey.equal(A.partkey);
        Bit eq2 = B.partkey.equal(C.partkey);
        Bit eq = eq1 | eq2;
        Integer partkey = integer_bit_and(B.partkey, eq);
        Integer volume = integer_bit_and((A.volume | B.volume), eq1) | integer_bit_and((B.volume | C.volume), eq2);
        Integer suppkey = integer_bit_and((A.suppkey | B.suppkey), eq1) | integer_bit_and((B.suppkey | C.suppkey), eq2);
        Integer orderkey =
            integer_bit_and((A.orderkey | B.orderkey), eq1) | integer_bit_and((B.orderkey | C.orderkey), eq2);

        irow_13 returnedRow = {.partkey = partkey, .volume = volume, .suppkey = suppkey, .orderkey = orderkey};
        return returnedRow;
    }

    inline irow_13 select_2(irow_13 A, irow_13 B) {
        Bit eq = B.partkey.equal(A.partkey);
        Integer partkey = integer_bit_and(B.partkey, eq);
        Integer volume = integer_bit_and((A.volume | B.volume), eq);
        Integer suppkey = integer_bit_and((A.suppkey | B.suppkey), eq);
        Integer orderkey = integer_bit_and((A.orderkey | B.orderkey), eq);

        irow_13 returnedRow = {.partkey = partkey, .volume = volume, .suppkey = suppkey, .orderkey = orderkey};
        return returnedRow;
    }

    inline irow_123 select_3(irow_123 A, irow_123 B, irow_123 C) {
        Bit eq1 = B.suppkey.equal(A.suppkey);
        Bit eq2 = B.suppkey.equal(C.suppkey);
        Bit eq = eq1 | eq2;
        Integer suppkey = integer_bit_and(B.suppkey, eq);
        Integer volume = integer_bit_and((A.volume | B.volume), eq1) | integer_bit_and((B.volume | C.volume), eq2);
        Integer nationkey =
            integer_bit_and((A.nationkey | B.nationkey), eq1) | integer_bit_and((B.nationkey | C.nationkey), eq2);
        Integer orderkey =
            integer_bit_and((A.orderkey | B.orderkey), eq1) | integer_bit_and((B.orderkey | C.orderkey), eq2);

        irow_123 returnedRow = {.volume = volume, .suppkey = suppkey, .orderkey = orderkey, .nationkey = nationkey};
        return returnedRow;
    }

    inline irow_123 select_2(irow_123 A, irow_123 B) {
        Bit eq = B.suppkey.equal(A.suppkey);
        Integer suppkey = integer_bit_and(B.suppkey, eq);
        Integer volume = integer_bit_and((A.volume | B.volume), eq);
        Integer nationkey = integer_bit_and((A.nationkey | B.nationkey), eq);
        Integer orderkey = integer_bit_and((A.orderkey | B.orderkey), eq);

        irow_123 returnedRow = {.volume = volume, .suppkey = suppkey, .orderkey = orderkey, .nationkey = nationkey};
        return returnedRow;
    }

    inline irow_45 select_3(irow_45 A, irow_45 B, irow_45 C) {
        Bit eq1 = B.custkey.equal(A.custkey);
        Bit eq2 = B.custkey.equal(C.custkey);
        Bit eq = eq1 | eq2;
        Integer custkey = integer_bit_and(B.custkey, eq);
        Integer orderkey =
            integer_bit_and((A.orderkey | B.orderkey), eq1) | integer_bit_and((B.orderkey | C.orderkey), eq2);
        Integer orderdate =
            integer_bit_and((A.orderdate | B.orderdate), eq1) | integer_bit_and((B.orderdate | C.orderdate), eq2);
        Integer nationkey =
            integer_bit_and((A.nationkey | B.nationkey), eq1) | integer_bit_and((B.nationkey | C.nationkey), eq2);

        irow_45 returnedRow = {
            .orderkey = orderkey, .orderdate = orderdate, .custkey = custkey, .nationkey = nationkey};
        return returnedRow;
    }

    inline irow_45 select_2(irow_45 A, irow_45 B) {
        Bit eq = B.custkey.equal(A.custkey);
        Integer custkey = integer_bit_and(B.custkey, eq);
        Integer orderkey = integer_bit_and((A.orderkey | B.orderkey), eq);
        Integer orderdate = integer_bit_and((A.orderdate | B.orderdate), eq);
        Integer nationkey = integer_bit_and((A.nationkey | B.nationkey), eq);

        irow_45 returnedRow = {
            .orderkey = orderkey, .orderdate = orderdate, .custkey = custkey, .nationkey = nationkey};
        return returnedRow;
    }

    inline irow_67 select_3(irow_67 A, irow_67 B, irow_67 C) {
        Bit eq1 = B.regionkey.equal(A.regionkey);
        Bit eq2 = B.regionkey.equal(C.regionkey);
        Bit eq = eq1 | eq2;
        Integer regionkey = integer_bit_and(B.regionkey, eq);
        Integer name = integer_bit_and((A.name | B.name), eq1) | integer_bit_and((B.name | C.name), eq2);
        Integer nationkey =
            integer_bit_and((A.nationkey | B.nationkey), eq1) | integer_bit_and((B.nationkey | C.nationkey), eq2);

        irow_67 returnedRow = {.nationkey = nationkey, .name = name, .regionkey = regionkey};
        return returnedRow;
    }

    inline irow_67 select_2(irow_67 A, irow_67 B) {
        Bit eq = B.regionkey.equal(A.regionkey);
        Integer regionkey = integer_bit_and(B.regionkey, eq);
        Integer name = integer_bit_and((A.name | B.name), eq);
        Integer nationkey = integer_bit_and((A.nationkey | B.nationkey), eq);

        irow_67 returnedRow = {.nationkey = nationkey, .name = name, .regionkey = regionkey};
        return returnedRow;
    }

    inline irow_4567 select_3(irow_4567 A, irow_4567 B, irow_4567 C) {
        Bit eq1 = B.nationkey.equal(A.nationkey);
        Bit eq2 = B.nationkey.equal(C.nationkey);
        Bit eq = eq1 | eq2;
        Integer nationkey = integer_bit_and(B.nationkey, eq);
        Integer name = integer_bit_and((A.name | B.name), eq1) | integer_bit_and((B.name | C.name), eq2);
        Integer orderkey =
            integer_bit_and((A.orderkey | B.orderkey), eq1) | integer_bit_and((B.orderkey | C.orderkey), eq2);
        Integer orderdate =
            integer_bit_and((A.orderdate | B.orderdate), eq1) | integer_bit_and((B.orderdate | C.orderdate), eq2);

        irow_4567 returnedRow = {.orderkey = orderkey, .orderdate = orderdate, .nationkey = nationkey, .name = name};
        return returnedRow;
    }

    inline irow_4567 select_2(irow_4567 A, irow_4567 B) {
        Bit eq = B.nationkey.equal(A.nationkey);
        Integer nationkey = integer_bit_and(B.nationkey, eq);
        Integer name = integer_bit_and((A.name | B.name), eq);
        Integer orderkey = integer_bit_and((A.orderkey | B.orderkey), eq);
        Integer orderdate = integer_bit_and((A.orderdate | B.orderdate), eq);

        irow_4567 returnedRow = {.orderkey = orderkey, .orderdate = orderdate, .nationkey = nationkey, .name = name};
        return returnedRow;
    }

    inline irow_1234567 select_3(irow_1234567 A, irow_1234567 B, irow_1234567 C) {
        Bit eq1 = B.nationkey.equal(A.nationkey) & B.orderkey.equal(A.orderkey);
        Bit eq2 = B.nationkey.equal(C.nationkey) & B.orderkey.equal(C.orderkey);
        Bit eq = eq1 | eq2;
        Integer nationkey = integer_bit_and(B.nationkey, eq);
        Integer orderkey = integer_bit_and(B.orderkey, eq);
        Integer name = integer_bit_and((A.name | B.name), eq1) | integer_bit_and((B.name | C.name), eq2);
        Integer volume = integer_bit_and((A.volume | B.volume), eq1) | integer_bit_and((B.volume | C.volume), eq2);
        Integer orderdate =
            integer_bit_and((A.orderdate | B.orderdate), eq1) | integer_bit_and((B.orderdate | C.orderdate), eq2);

        irow_1234567 returnedRow = {
            .volume = volume, .orderkey = orderkey, .orderdate = orderdate, .nationkey = nationkey, .name = name};
        return returnedRow;
    }

    inline irow_1234567 select_2(irow_1234567 A, irow_1234567 B) {
        Bit eq1 = B.nationkey.equal(A.nationkey);
        Bit eq2 = B.orderkey.equal(A.orderkey);
        Bit eq = eq1 & eq2;
        Integer nationkey = integer_bit_and(B.nationkey, eq);
        Integer orderkey = integer_bit_and(B.orderkey, eq);
        Integer name = integer_bit_and((A.name | B.name), eq);
        Integer volume = integer_bit_and((A.volume | B.volume), eq);
        Integer orderdate = integer_bit_and((A.orderdate | B.orderdate), eq);

        irow_1234567 returnedRow = {
            .volume = volume, .orderkey = orderkey, .orderdate = orderdate, .nationkey = nationkey, .name = name};
        return returnedRow;
    }

    template <std::size_t width>
    void create_1_3(std::size_t problem_size, const std::vector<Integer>& input_data_1,
                    const std::vector<Integer>& input_data_2, std::vector<orow_13>& output_data) {
        int input_length_1 = problem_size;
        int input_length_2 = problem_size;

        int tot_length = input_length_1 + input_length_2;
        int min_length = input_length_1 < input_length_2 ? input_length_1 : input_length_2;

        int cols_1 = input_data_1.size() / input_length_1;
        int cols_2 = input_data_2.size() / input_length_2;

        std::vector<Integer> input(input_data_1);
        input.insert(input.end(), input_data_2.begin(), input_data_2.end());

        std::vector<irow_13> rows(tot_length);

        for (int i = 0; i < input_length_1; i++) {
            rows[i].partkey = input[cols_1 * i];
            rows[i].volume = Integer(width, 0, PUBLIC);
            rows[i].suppkey = Integer(width, 0, PUBLIC);
            rows[i].orderkey = Integer(width, 0, PUBLIC);
        }

        for (int i = 0; i < input_length_2; i++) {
            rows[input_length_1 + i].partkey = input[cols_1 * input_length_1 + cols_2 * i];
            rows[input_length_1 + i].volume = input[cols_1 * input_length_1 + cols_2 * i + 1];
            rows[input_length_1 + i].suppkey = input[cols_1 * input_length_1 + cols_2 * i + 2];
            rows[input_length_1 + i].orderkey = input[cols_1 * input_length_1 + cols_2 * i + 3];
        }

        // Verify input keys are sorted
        Bit verifyOrder(true);
        for (int i = 0; i < input_length_1 - 1; ++i) {
            Bit lessThanNext = rows[i].partkey.geq(rows[i + 1].partkey);
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = input_length_1; i < tot_length - 1; ++i) {
            Bit greaterThanNext = rows[i].partkey.geq(rows[i + 1].partkey);
            verifyOrder = verifyOrder & greaterThanNext;
        }

        std::cout << "Sort verification done" << std::endl;

        // PSI
        std::vector<irow_13> psi_output_rows(tot_length / 2);
        keyMerge(rows, 0, tot_length);
        for (int i = 0; i < tot_length - 2; i += 2) {
            psi_output_rows[i / 2] = select_3(rows[i], rows[i + 1], rows[i + 2]);
        }
        psi_output_rows[tot_length / 2 - 1] = select_2(rows[tot_length - 2], rows[tot_length - 1]);

        std::cout << "PSI done" << std::endl;

        // Project required columns
        std::vector<orow_13> output_rows(tot_length / 2);
        for (int i = 0; i < tot_length / 2; i++) {
            output_rows[i].volume = psi_output_rows[i].volume;
            output_rows[i].suppkey = psi_output_rows[i].suppkey;
            output_rows[i].orderkey = psi_output_rows[i].orderkey;
        }

        // Sort
        keySort(output_rows, 0, tot_length / 2);
        std::cout << "Sort done" << std::endl;

        output_data.clear();
        output_data.insert(output_data.end(), output_rows.begin(), output_rows.end());

        return;
    }

    template <std::size_t width>
    void create_1_2_3(std::size_t problem_size, const std::vector<orow_13>& input_data_1,
                      const std::vector<Integer>& input_data_2, std::vector<orow_123>& output_data) {
        int input_length_1 = problem_size;
        int input_length_2 = problem_size;

        int tot_length = input_length_1 + input_length_2;
        int min_length = input_length_1 < input_length_2 ? input_length_1 : input_length_2;

        int cols_1 = 3;
        int cols_2 = input_data_2.size() / input_length_2;
        std::vector<Integer> input(cols_1 * input_length_1);
        for (int i = 0; i < input_length_1; i++) {
            input[cols_1 * i] = input_data_1[i].volume;
            input[cols_1 * i + 1] = input_data_1[i].suppkey;
            input[cols_1 * i + 2] = input_data_1[i].orderkey;
        }
        input.insert(input.end(), input_data_2.begin(), input_data_2.end());

        std::vector<irow_123> rows(tot_length);

        for (int i = 0; i < input_length_1; i++) {
            rows[i].volume = input[cols_1 * i];
            rows[i].suppkey = input[cols_1 * i + 1];
            rows[i].orderkey = input[cols_1 * i + 2];
            rows[i].nationkey = Integer(width, 0, PUBLIC);
        }

        for (int i = 0; i < input_length_2; i++) {
            rows[input_length_1 + i].volume = ZERO;
            rows[input_length_1 + i].suppkey = input[cols_1 * input_length_1 + cols_2 * i];
            rows[input_length_1 + i].orderkey = ZERO;
            rows[input_length_1 + i].nationkey = input[cols_1 * input_length_1 + cols_2 * i + 1];
        }

        // Verify input keys are sorted
        Bit verifyOrder(true);
        for (int i = 0; i < input_length_1 - 1; ++i) {
            Bit lessThanNext = rows[i].suppkey.geq(rows[i + 1].suppkey);
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = input_length_1; i < tot_length - 1; ++i) {
            Bit greaterThanNext = rows[i].suppkey.geq(rows[i + 1].suppkey);
            verifyOrder = verifyOrder & greaterThanNext;
        }

        std::cout << "Sort verification done" << std::endl;

        // PSI
        std::vector<irow_123> psi_output_rows(tot_length / 2);
        keyMerge(rows, 0, tot_length);
        for (int i = 0; i < tot_length - 2; i += 2) {
            psi_output_rows[i / 2] = select_3(rows[i], rows[i + 1], rows[i + 2]);
        }
        psi_output_rows[tot_length / 2 - 1] = select_2(rows[tot_length - 2], rows[tot_length - 1]);

        std::cout << "PSI done" << std::endl;

        // Project required columns
        std::vector<orow_123> output_rows(tot_length / 2);
        for (int i = 0; i < tot_length / 2; i++) {
            output_rows[i].volume = psi_output_rows[i].volume;
            output_rows[i].orderkey = psi_output_rows[i].orderkey;
            output_rows[i].nationkey = psi_output_rows[i].nationkey;
        }

        // Sort
        keySort(output_rows, 0, tot_length / 2);
        std::cout << "Sort done" << std::endl;

        output_data.clear();
        output_data.insert(output_data.end(), output_rows.begin(), output_rows.end());

        return;
    }

    template <std::size_t width>
    void create_4_5(std::size_t problem_size, const std::vector<Integer>& input_data_1,
                    const std::vector<Integer>& input_data_2, std::vector<orow_45>& output_data) {
        int input_length_1 = problem_size;
        int input_length_2 = problem_size;

        int tot_length = input_length_1 + input_length_2;
        int min_length = input_length_1 < input_length_2 ? input_length_1 : input_length_2;

        int cols_1 = input_data_1.size() / input_length_1;
        int cols_2 = input_data_2.size() / input_length_2;
        std::vector<Integer> input(cols_1 * input_length_1 + cols_2 * input_length_2);
        for (int i = 0; i < cols_1 * input_length_1 + cols_2 * input_length_2; i++) {
            input[i] = Integer(width, 0, ALICE);
        }

        std::vector<irow_45> rows(tot_length);

        for (int i = 0; i < input_length_1; i++) {
            rows[i].orderkey = input[cols_1 * i];
            rows[i].orderdate = input[cols_1 * i + 1];
            rows[i].custkey = input[cols_1 * i + 2];
            rows[i].nationkey = ZERO;
        }

        for (int i = 0; i < input_length_2; i++) {
            rows[input_length_1 + i].orderkey = ZERO;
            rows[input_length_1 + i].orderdate = ZERO;
            rows[input_length_1 + i].custkey = input[cols_1 * input_length_1 + cols_2 * i];
            rows[input_length_1 + i].nationkey = input[cols_1 * input_length_1 + cols_2 * i + 1];
        }

        // Verify input keys are sorted
        Bit verifyOrder(true);
        for (int i = 0; i < input_length_1 - 1; ++i) {
            Bit lessThanNext = rows[i].custkey.geq(rows[i + 1].custkey);
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = input_length_1; i < tot_length - 1; ++i) {
            Bit greaterThanNext = rows[i].custkey.geq(rows[i + 1].custkey);
            verifyOrder = verifyOrder & greaterThanNext;
        }

        std::cout << "Sort verification done" << std::endl;

        // PSI
        std::vector<irow_45> psi_output_rows(tot_length / 2);
        keyMerge(rows, 0, tot_length);
        for (int i = 0; i < tot_length - 2; i += 2) {
            psi_output_rows[i / 2] = select_3(rows[i], rows[i + 1], rows[i + 2]);
        }
        psi_output_rows[tot_length / 2 - 1] = select_2(rows[tot_length - 2], rows[tot_length - 1]);

        std::cout << "PSI done" << std::endl;

        // Project required columns
        std::vector<orow_45> output_rows(tot_length / 2);
        for (int i = 0; i < tot_length / 2; i++) {
            output_rows[i].orderkey = psi_output_rows[i].orderkey;
            output_rows[i].orderdate = psi_output_rows[i].orderdate;
            output_rows[i].nationkey = psi_output_rows[i].nationkey;
        }

        // Sort
        keySort(output_rows, 0, tot_length / 2);
        std::cout << "Sort done" << std::endl;

        output_data.clear();
        output_data.insert(output_data.end(), output_rows.begin(), output_rows.end());

        return;
    }

    template <std::size_t width>
    void create_6_7(std::size_t problem_size, const std::vector<Integer>& input_data_1,
                    const std::vector<Integer>& input_data_2, std::vector<orow_67>& output_data) {
        std::size_t input_length_1 = problem_size;
        std::size_t input_length_2 = problem_size;

        int tot_length = input_length_1 + input_length_2;
        int min_length = input_length_1 < input_length_2 ? input_length_1 : input_length_2;

        int cols_1 = input_data_1.size() / input_length_1;
        int cols_2 = input_data_2.size() / input_length_2;
        std::vector<Integer> input(cols_1 * input_length_1 + cols_2 * input_length_2);
        for (int i = 0; i < cols_1 * input_length_1 + cols_2 * input_length_2; i++) {
            input[i] = Integer(width, 0, ALICE);
        }

        std::vector<irow_67> rows(tot_length);

        for (int i = 0; i < input_length_1; i++) {
            rows[i].nationkey = input[cols_1 * i];
            rows[i].name = input[cols_1 * i + 1];
            rows[i].regionkey = input[cols_1 * i + 2];
        }

        for (int i = 0; i < input_length_2; i++) {
            rows[input_length_1 + i].nationkey = ZERO;
            rows[input_length_1 + i].name = ZERO;
            rows[input_length_1 + i].regionkey = input[cols_1 * input_length_1 + cols_2 * i];
        }

        // Verify input keys are sorted
        Bit verifyOrder(true);
        for (int i = 0; i < input_length_1 - 1; ++i) {
            Bit lessThanNext = rows[i].regionkey.geq(rows[i + 1].regionkey);
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = input_length_1; i < tot_length - 1; ++i) {
            Bit greaterThanNext = rows[i].regionkey.geq(rows[i + 1].regionkey);
            verifyOrder = verifyOrder & greaterThanNext;
        }

        std::cout << "Sort verification done" << std::endl;

        // PSI
        std::vector<irow_67> psi_output_rows(tot_length / 2);
        keyMerge(rows, 0, tot_length);
        for (int i = 0; i < tot_length - 2; i += 2) {
            psi_output_rows[i / 2] = select_3(rows[i], rows[i + 1], rows[i + 2]);
        }
        psi_output_rows[tot_length / 2 - 1] = select_2(rows[tot_length - 2], rows[tot_length - 1]);

        std::cout << "PSI done" << std::endl;

        // Project required columns
        std::vector<orow_67> output_rows(tot_length / 2);
        for (int i = 0; i < tot_length / 2; i++) {
            output_rows[i].nationkey = psi_output_rows[i].nationkey;
            output_rows[i].name = psi_output_rows[i].name;
        }

        // Sort
        keySort(output_rows, 0, tot_length / 2);
        std::cout << "Sort done" << std::endl;

        output_data.clear();
        output_data.insert(output_data.end(), output_rows.begin(), output_rows.end());

        return;
    }

    template <std::size_t width>
    void create_4_5_6_7(std::size_t problem_size, const std::vector<orow_45>& input_data_1,
                        const std::vector<orow_67>& input_data_2, std::vector<orow_4567>& output_data) {
        int input_length_1 = problem_size;
        int input_length_2 = problem_size;

        int tot_length = input_length_1 + input_length_2;
        int min_length = input_length_1 < input_length_2 ? input_length_1 : input_length_2;

        int cols_1 = 3;
        int cols_2 = 2;
        std::vector<Integer> input(cols_1 * input_length_1 + cols_2 * input_length_2);
        for (int i = 0; i < input_length_1; i++) {
            input[cols_1 * i] = input_data_1[i].orderkey;
            input[cols_1 * i + 1] = input_data_1[i].orderdate;
            input[cols_1 * i + 2] = input_data_1[i].nationkey;
        }
        for (int i = 0; i < input_length_2; i++) {
            input[cols_1 * input_length_1 + cols_2 * i] = input_data_2[i].nationkey;
            input[cols_1 * input_length_1 + cols_2 * i + 1] = input_data_2[i].name;
        }

        std::vector<irow_4567> rows(tot_length);

        for (int i = 0; i < input_length_1; i++) {
            rows[i].orderkey = input[cols_1 * i];
            rows[i].orderdate = input[cols_1 * i + 1];
            rows[i].nationkey = input[cols_1 * i + 2];
            rows[i].name = ZERO;
        }

        for (int i = 0; i < input_length_2; i++) {
            rows[input_length_1 + i].orderkey = ZERO;
            rows[input_length_1 + i].orderdate = ZERO;
            rows[input_length_1 + i].nationkey = input[cols_1 * input_length_1 + cols_2 * i];
            rows[input_length_1 + i].name = input[cols_1 * input_length_1 + cols_2 * i + 1];
        }

        // Verify input keys are sorted
        Bit verifyOrder(true);
        for (int i = 0; i < input_length_1 - 1; ++i) {
            Bit lessThanNext = rows[i].nationkey.geq(rows[i + 1].nationkey);
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = input_length_1; i < tot_length - 1; ++i) {
            Bit greaterThanNext = rows[i].nationkey.geq(rows[i + 1].nationkey);
            verifyOrder = verifyOrder & greaterThanNext;
        }

        std::cout << "Sort verification done" << std::endl;

        // PSI
        std::vector<irow_4567> psi_output_rows(tot_length / 2);
        keyMerge(rows, 0, tot_length);
        for (int i = 0; i < tot_length - 2; i += 2) {
            psi_output_rows[i / 2] = select_3(rows[i], rows[i + 1], rows[i + 2]);
        }
        psi_output_rows[tot_length / 2 - 1] = select_2(rows[tot_length - 2], rows[tot_length - 1]);

        std::cout << "PSI done" << std::endl;

        // Project required columns
        std::vector<orow_4567> output_rows(tot_length / 2);
        for (int i = 0; i < tot_length / 2; i++) {
            output_rows[i].orderkey = psi_output_rows[i].orderkey;
            output_rows[i].orderdate = psi_output_rows[i].orderdate;
            output_rows[i].nationkey = psi_output_rows[i].nationkey;
            output_rows[i].name = psi_output_rows[i].name;
        }

        // Sort
        keySort(output_rows, 0, tot_length / 2);
        std::cout << "Sort done" << std::endl;

        output_data.clear();
        output_data.insert(output_data.end(), output_rows.begin(), output_rows.end());

        return;
    }

    template <std::size_t width>
    void create_1_2_3_4_5_6_7(std::size_t problem_size, const std::vector<orow_123>& input_data_1,
                              const std::vector<orow_4567>& input_data_2, std::vector<orow_1234567>& output_data) {
        std::size_t input_length_1 = problem_size;
        std::size_t input_length_2 = problem_size;

        int tot_length = input_length_1 + input_length_2;
        int min_length = input_length_1 < input_length_2 ? input_length_1 : input_length_2;

        int cols_1 = 3;
        int cols_2 = 4;
        std::vector<Integer> input(cols_1 * input_length_1 + cols_2 * input_length_2);
        for (int i = 0; i < input_length_1; i++) {
            input[cols_1 * i] = input_data_1[i].volume;
            input[cols_1 * i + 1] = input_data_1[i].orderkey;
            input[cols_1 * i + 2] = input_data_1[i].nationkey;
        }
        for (int i = 0; i < input_length_2; i++) {
            input[cols_1 * input_length_1 + cols_2 * i] = input_data_2[i].orderkey;
            input[cols_1 * input_length_1 + cols_2 * i + 1] = input_data_2[i].orderdate;
            input[cols_1 * input_length_1 + cols_2 * i + 2] = input_data_2[i].nationkey;
            input[cols_1 * input_length_1 + cols_2 * i + 3] = input_data_2[i].name;
        }

        std::vector<irow_1234567> rows(tot_length);

        for (int i = 0; i < input_length_1; i++) {
            rows[i].volume = input[cols_1 * i];
            rows[i].orderkey = input[cols_1 * i + 1];
            rows[i].orderdate = ZERO;
            rows[i].nationkey = input[cols_1 * i + 2];
            rows[i].name = ZERO;
        }

        std::cout << "Assignment1 done" << std::endl;

        for (int i = 0; i < input_length_2; i++) {
            rows[input_length_1 + i].volume = ZERO;
            rows[input_length_1 + i].orderkey = input[cols_1 * input_length_1 + cols_2 * i];
            rows[input_length_1 + i].orderdate = input[cols_1 * input_length_1 + cols_2 * i + 1];
            rows[input_length_1 + i].nationkey = input[cols_1 * input_length_1 + cols_2 * i + 2];
            rows[input_length_1 + i].name = input[cols_1 * input_length_1 + cols_2 * i + 3];
        }
        std::cout << "Assignment2 done" << std::endl;

        // Verify input keys are sorted
        Bit verifyOrder(true);
        for (int i = 0; i < input_length_1 - 1; ++i) {
            Bit lessThanNext1 = rows[i].nationkey.geq(rows[i + 1].nationkey);
            Bit lessThanNext2 = rows[i].orderkey.geq(rows[i + 1].orderkey);
            Bit lessThanNext = lessThanNext1 & lessThanNext2;
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = input_length_1; i < tot_length - 1; ++i) {
            Bit lessThanNext1 = rows[i].nationkey.geq(rows[i + 1].nationkey);
            Bit lessThanNext2 = rows[i].orderkey.geq(rows[i + 1].orderkey);
            Bit greaterThanNext = lessThanNext1 & lessThanNext2;
            verifyOrder = verifyOrder & greaterThanNext;
        }

        std::cout << "Sort verification done" << std::endl;

        // PSI
        std::vector<irow_1234567> psi_output_rows(tot_length / 2);
        keyMerge(rows, 0, tot_length);
        for (int i = 0; i < tot_length - 2; i += 2) {
            psi_output_rows[i / 2] = select_3(rows[i], rows[i + 1], rows[i + 2]);
        }
        psi_output_rows[tot_length / 2 - 1] = select_2(rows[tot_length - 2], rows[tot_length - 1]);

        std::cout << "PSI done" << std::endl;

        // Project required columns
        std::vector<orow_1234567> output_rows(tot_length / 2);
        for (int i = 0; i < tot_length / 2; i++) {
            output_rows[i].orderdate = psi_output_rows[i].orderdate;
            output_rows[i].volume = psi_output_rows[i].volume;
            output_rows[i].name = psi_output_rows[i].name;
        }

        // Sort
        keySort(output_rows, 0, tot_length / 2);
        std::cout << "Sort done" << std::endl;

        // Groupby sum
        for (int i = 0; i < min_length - 1; ++i) {
            Bit equals = output_rows[i].orderdate.equal(output_rows[i + 1].orderdate);
            output_rows[i].orderdate = output_rows[i].orderdate.select(equals, Integer(width, 0, PUBLIC));
        }
        std::cout << "Groupby done" << std::endl;

        Integer agg(width, 0, PUBLIC);
        for (int i = 1; i < input_length_2; ++i) {
            Bit equals1 = output_rows[i].orderdate.equal(Integer(width, 0, PUBLIC));
            Bit equals2 = output_rows[i].name.equal(Integer(width, 10, PUBLIC));
            output_rows[i].volume =
                integer_bit_and(integer_bit_and(output_rows[i].volume + output_rows[i - 1].volume, equals2), equals1);
        }
        std::cout << "Sum done" << std::endl;

        output_data.clear();
        output_data.insert(output_data.end(), output_rows.begin(), output_rows.end());

        return;
    }

    std::size_t get_other_input_size(int party, std::size_t problem_size) {
        if (party == 1) {
            return 8 * problem_size;
        } else {
            return 8 * problem_size;
        }
    }

    template <std::size_t width>
    void join_and_aggregate(int party, std::size_t problem_size, const std::vector<Integer>& input_data,
                            std::vector<Integer>& output_data) {
        int c_1 = 1;
        int c_2 = 2;
        int c_3 = 4;
        int c_4 = 3;
        int c_5 = 2;
        int c_6 = 3;
        int c_7 = 1;

        std::vector<Integer>::const_iterator input_data_iter = input_data.begin();
        std::vector<Integer> input_data_1(input_data_iter, input_data_iter = input_data_iter + c_1 * problem_size);
        std::vector<Integer> input_data_2(input_data_iter, input_data_iter = input_data_iter + c_2 * problem_size);
        std::vector<Integer> input_data_3(input_data_iter, input_data_iter = input_data_iter + c_3 * problem_size);
        std::vector<Integer> input_data_4(input_data_iter, input_data_iter = input_data_iter + c_4 * problem_size);
        std::vector<Integer> input_data_5(input_data_iter, input_data_iter = input_data_iter + c_5 * problem_size);
        std::vector<Integer> input_data_6(input_data_iter, input_data_iter = input_data_iter + c_6 * problem_size);
        std::vector<Integer> input_data_7(input_data_iter, input_data_iter = input_data_iter + c_7 * problem_size);

        std::vector<orow_13> output_data_1_3(problem_size);
        std::vector<orow_45> output_data_4_5(problem_size);
        std::vector<orow_67> output_data_6_7(problem_size);
        std::vector<orow_123> output_data_1_2_3(problem_size);
        std::vector<orow_4567> output_data_4_5_6_7(problem_size);
        std::vector<orow_1234567> output_data_1_2_3_4_5_6_7(problem_size);

        create_1_3<width>(problem_size, input_data_1, input_data_3, output_data_1_3);
        create_4_5<width>(problem_size, input_data_4, input_data_5, output_data_4_5);
        create_6_7<width>(problem_size, input_data_6, input_data_7, output_data_6_7);
        create_1_2_3<width>(problem_size, output_data_1_3, input_data_2, output_data_1_2_3);
        create_4_5_6_7<width>(problem_size, output_data_4_5, output_data_6_7, output_data_4_5_6_7);
        create_1_2_3_4_5_6_7<width>(problem_size, output_data_1_2_3, output_data_4_5_6_7, output_data_1_2_3_4_5_6_7);

        output_data.clear();
        for (const orow_1234567& row : output_data_1_2_3_4_5_6_7) {
            output_data.push_back(row.orderdate);
            output_data.push_back(row.volume);
        }
    }

}
