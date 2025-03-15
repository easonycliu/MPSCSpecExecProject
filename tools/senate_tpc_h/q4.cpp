namespace senate_tpc_h_q4 {

    typedef struct _KeyVal {
        Integer key;
        Integer count;
    } KeyVal;

    typedef struct _row_12 {
        Integer orderKey;
        Integer orderPriority;
        Integer count;
    } row_12;

    typedef struct _row_l {
        Integer orderKey;
        Integer commitDate;
        Integer receiptDate;
    } row_l;

    typedef struct _row_o {
        Integer orderKey;
        Integer orderDate;
        Integer orderPriority;
    } row_o;

    typedef struct _row_123_o {
        Integer orderPriority;
        Integer count;
    } row_123_o;

    void cmp_swap(std::vector<KeyVal>& group, int i, int j) {
        Bit to_swap = (group[i].key > group[j].key);
        swap(to_swap, group[i].key, group[j].key);
        swap(to_swap, group[i].count, group[j].count);
    }

    void cmp_swap(std::vector<row_12>& group, int i, int j) {
        Bit to_swap = (group[i].orderPriority > group[j].orderPriority);
        swap(to_swap, group[i].orderKey, group[j].orderKey);
        swap(to_swap, group[i].orderPriority, group[j].orderPriority);
        swap(to_swap, group[i].count, group[j].count);
    }

    void groupBy(std::vector<KeyVal>& group, int lo, int n) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++)
                cmp_swap(group, i, i + m);
            groupBy(group, lo, n - m);
            groupBy(group, lo + n - m, m);
        }
    }

    void groupBy(std::vector<row_12>& group, int lo, int n) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++)
                cmp_swap(group, i, i + m);
            groupBy(group, lo, n - m);
            groupBy(group, lo + n - m, m);
        }
    }

    void cmp_swap_counts(std::vector<KeyVal>& group, int i, int j, Bit acc) {
        Bit to_swap = ((group[i].count > group[j].count) == acc);
        swap(to_swap, group[i].key, group[j].key);
        swap(to_swap, group[i].count, group[j].count);
    }

    void cmp_swap_key(std::vector<row_12>& group, int i, int j, Bit acc) {
        Bit to_swap = ((group[i].orderKey > group[j].orderKey) == acc);
        swap(to_swap, group[i].orderKey, group[j].orderKey);
        swap(to_swap, group[i].orderPriority, group[j].orderPriority);
        swap(to_swap, group[i].count, group[j].count);
    }

    void cmp_swap_key(std::vector<row_o>& group, int i, int j, Bit acc) {
        Bit to_swap = ((group[i].orderKey > group[j].orderKey) == acc);
        swap(to_swap, group[i].orderKey, group[j].orderKey);
        swap(to_swap, group[i].orderDate, group[j].orderDate);
        swap(to_swap, group[i].orderPriority, group[j].orderPriority);
    }

    void cmp_swap_key(std::vector<row_l>& group, int i, int j, Bit acc) {
        Bit to_swap = ((group[i].orderKey > group[j].orderKey) == acc);
        swap(to_swap, group[i].orderKey, group[j].orderKey);
        swap(to_swap, group[i].commitDate, group[j].commitDate);
        swap(to_swap, group[i].receiptDate, group[j].receiptDate);
    }

    void cmp_swap_key(std::vector<row_123_o>& group, int i, int j, Bit acc) {
        Bit to_swap = ((group[i].orderPriority > group[j].orderPriority) == acc);
        swap(to_swap, group[i].orderPriority, group[j].orderPriority);
        swap(to_swap, group[i].count, group[j].count);
    }

    void merge(std::vector<row_12>& group, int lo, int n, bool acc) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++) {
                cmp_swap_key(group, i, i + m, acc);
            }
            if (acc) {
                merge(group, lo, n - m, acc);
                merge(group, lo + n - m, m, acc);
            } else {
                merge(group, lo, m, acc);
                merge(group, lo + m, n - m, acc);
            }
        }
    }

    void merge(std::vector<row_o>& group, int lo, int n, bool acc) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++) {
                cmp_swap_key(group, i, i + m, acc);
            }
            if (acc) {
                merge(group, lo, n - m, acc);
                merge(group, lo + n - m, m, acc);
            } else {
                merge(group, lo, m, acc);
                merge(group, lo + m, n - m, acc);
            }
        }
    }

    void merge(std::vector<row_l>& group, int lo, int n, bool acc) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++) {
                cmp_swap_key(group, i, i + m, acc);
            }
            if (acc) {
                merge(group, lo, n - m, acc);
                merge(group, lo + n - m, m, acc);
            } else {
                merge(group, lo, m, acc);
                merge(group, lo + m, n - m, acc);
            }
        }
    }

    void merge(std::vector<row_123_o>& group, int lo, int n, bool acc) {
        if (n > 1) {
            int m = greatestPowerOfTwoLessThan(n);
            for (int i = lo; i < lo + n - m; i++) {
                cmp_swap_key(group, i, i + m, acc);
            }
            if (acc) {
                merge(group, lo, n - m, acc);
                merge(group, lo + n - m, m, acc);
            } else {
                merge(group, lo, m, acc);
                merge(group, lo + m, n - m, acc);
            }
        }
    }

    void orderBy(std::vector<row_123_o>& group, int lo, int n, bool acc = true) {
        if (n > 1) {
            int m = n / 2;
            orderBy(group, lo, m, true);
            orderBy(group, lo + m, n - m, false);
            merge(group, lo, n, acc);
        }
    }

    void orderBy(std::vector<row_12>& group, int lo, int n, bool acc = true) {
        if (n > 1) {
            int m = n / 2;
            orderBy(group, lo, m, true);
            orderBy(group, lo + m, n - m, false);
            merge(group, lo, n, acc);
        }
    }

    void orderBy(std::vector<row_l>& group, int lo, int n, bool acc = true) {
        if (n > 1) {
            int m = n / 2;
            orderBy(group, lo, m, true);
            orderBy(group, lo + m, n - m, false);
            merge(group, lo, n, acc);
        }
    }

    void orderBy(std::vector<row_o>& group, int lo, int n, bool acc = true) {
        if (n > 1) {
            int m = n / 2;
            orderBy(group, lo, m, true);
            orderBy(group, lo + m, n - m, false);
            merge(group, lo, n, acc);
        }
    }

    inline Integer select(Integer A, Integer B) {
        Bit eq = A.equal(Integer(32, 0, PUBLIC));
        Integer result = A.select(eq, B);
        return result;
    }

    inline Integer dup_select_3(Integer A, Integer B, Integer C) {
        Bit eq1 = A.equal(B);
        Bit eq2 = B.equal(C);
        Bit eq = eq1 | eq2;
        Integer result = B;
        result[0] = result[0] & eq;
        return result;
    }

    inline Integer dup_select_2(Integer A, Integer B) {
        Bit eq = A.equal(B);
        Integer result = A;
        result[0] = result[0] & eq;
        return result;
    }

    inline row_12 dup_select_2(row_12 A, row_12 B) {
        Bit eq = A.orderKey.equal(B.orderKey);
        Integer resultKey = A.orderKey;
        Integer resultPriority = A.orderPriority | B.orderPriority;
        resultKey[0] = resultKey[0] & eq;
        resultPriority[0] = (A.orderPriority[0] & eq) | (B.orderPriority[0] & eq);
        row_12 res;
        res.orderKey = resultKey;
        res.orderPriority = resultPriority;
        res.count = A.count;
        return res;
    }

    inline row_12 dup_select_3(row_12 A, row_12 B, row_12 C) {
        Bit eq1 = A.orderKey.equal(B.orderKey);
        Bit eq2 = B.orderKey.equal(C.orderKey);
        Bit eq = eq1 | eq2;
        Integer resultKey = B.orderKey;
        Integer potentialPriority = C.orderPriority.select(eq1, A.orderPriority);
        Integer resultPriority = B.orderPriority | potentialPriority;
        resultKey[0] = resultKey[0] & eq;
        resultPriority[0] = resultPriority[0] & eq;

        row_12 res;
        res.orderKey = resultKey;
        res.orderPriority = resultPriority;
        res.count = A.count;
        return res;
    }

    std::size_t get_other_input_size(int party, std::size_t problem_size) {
        return 0;
    }

    // join o_order_key on l_order_key
    template <std::size_t width>
    void join_and_aggregate(int party, std::size_t problem_size, const std::vector<Integer>& input_data,
                            std::vector<Integer>& output_data) {
        // Party 1 has (orderKey, orderPriority)
        // Party 2 has l_orderKey
        std::size_t input_size = 2 * problem_size;
        std::vector<Integer> input1(std::begin(input_data), std::begin(input_data) + input_size);
        std::vector<Integer> input2(std::begin(input_data) + input_size, std::end(input_data));

        std::vector<row_12> groups_o(problem_size);
        for (int i = 0; i < 2 * problem_size; i += 2) {
            groups_o[i / 2].orderKey = input1[i];
            groups_o[i / 2].orderPriority = input1[i + 1];
            groups_o[i / 2].count = Integer(width, 1, PUBLIC);
        }

        // orderBy(groups_o, 0, problem_size, true);

        std::vector<row_12> groups_l(problem_size);
        for (int i = 0; i < problem_size; i++) {
            groups_l[i].orderKey = input2[i];
            groups_l[i].orderPriority = Integer(width, 0, PUBLIC);
            groups_l[i].count = Integer(width, 1, PUBLIC);
        }

        // orderBy(groups_l, 0, problem_size, false);

        std::vector<row_12> groups_join(2 * problem_size);
        for (int i = 0; i < problem_size; i++) {
            groups_join[i].orderKey = groups_o[i].orderKey;
            groups_join[i].orderPriority = groups_o[i].orderPriority;
            groups_join[i].count = groups_o[i].count;
        }

        for (int i = 0; i < problem_size; i++) {
            groups_join[problem_size + i].orderKey = groups_o[i].orderKey;
            groups_join[problem_size + i].orderPriority = groups_o[i].orderPriority;
            groups_join[problem_size + i].count = groups_o[i].count;
        }

        std::cout << "Verify inputs are sorted" << std::endl;

        Bit verifyOrder(true);
        for (int i = 0; i < problem_size - 1; i++) {
            Bit lessThanNext = groups_join[i].orderKey.geq(groups_join[i + 1].orderKey);
            verifyOrder = verifyOrder & !lessThanNext;
        }

        for (int i = problem_size; i < 2 * problem_size - 1; i++) {
            Bit greaterThanNext = groups_join[i].orderKey.geq(groups_join[i + 1].orderKey);
            verifyOrder = verifyOrder & greaterThanNext;
        }

        merge(groups_join, 0, 2 * problem_size, true);

        std::vector<row_12> output_groups_join(problem_size);

        // Find the output.
        for (int i = 0; i < 2 * problem_size - 2; i += 2) {
            output_groups_join[i / 2] = dup_select_3(groups_join[i], groups_join[i + 1], groups_join[i + 2]);
        }
        output_groups_join[problem_size - 1] =
            dup_select_2(groups_join[2 * problem_size - 2], groups_join[2 * problem_size - 1]);

        std::cout << "Do a filter, exists operator" << std::endl;

        for (int i = 0; i < problem_size; i++) {
            Bit keep_row = output_groups_join[i].orderKey != Integer(width, 0, PUBLIC);
            output_groups_join[i].orderKey = Integer(width, 0, PUBLIC).select(keep_row, output_groups_join[i].orderKey);
            output_groups_join[i].orderPriority =
                Integer(width, 0, PUBLIC).select(keep_row, output_groups_join[i].orderPriority);
            output_groups_join[i].orderKey = Integer(width, 0, PUBLIC).select(keep_row, output_groups_join[i].orderKey);
        }

        std::cout << "Do groupby on the resulting group" << std::endl;
        groupBy(output_groups_join, 0, problem_size);
        // This is the final round, do the sum and then reveal the results.
        for (int i = 0; i < problem_size - 1; i++) {
            Integer sum = output_groups_join[i].count + output_groups_join[i + 1].count;
            Bit equals = output_groups_join[i].orderPriority.equal(output_groups_join[i + 1].orderPriority);
            output_groups_join[i].orderKey = output_groups_join[i].orderKey.select(equals, Integer(width, 0, PUBLIC));
            output_groups_join[i].orderPriority =
                output_groups_join[i].orderPriority.select(equals, Integer(width, 0, PUBLIC));

            // Store resulting revenue inside extendedPrice
            output_groups_join[i].count = output_groups_join[i].count.select(equals, Integer(width, 0, PUBLIC));
            output_groups_join[i + 1].count = output_groups_join[i + 1].count.select(equals, sum);
        }

        // Copy into output groups

        std::cout << "Sorting output" << std::endl;
        std::vector<row_123_o> final_groups(problem_size);
        for (int i = 0; i < problem_size; i++) {
            final_groups[i].orderPriority = output_groups_join[i].orderPriority;
            final_groups[i].count = output_groups_join[i].count;
        }

        orderBy(final_groups, 0, problem_size, true);

        for (int i = 0; i < problem_size; i++) {
            output_data.push_back(final_groups[i].orderPriority);
            output_data.push_back(final_groups[i].count);
        }

        return;
    }

}
