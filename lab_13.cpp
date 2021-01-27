#include <iostream>
#include <vector>
#include <random>

using namespace std;

int fibonacci(int n) {
    if (n == 0 || n == 1) {
        return n;
    }
    return fibonacci(n-2) + fibonacci(n-1);
}

#define RESULT_TAG 1
#define TASK_TAG 2

int empty_task() { return -1; }

bool is_empty_task(int task) { return -1 == task; }

void coordinate_computations(int processes) {
    uint32_t N = 20;
    vector<int> tasks(N);

    default_random_engine generator;
    uniform_int_distribution<int> distribution(30, 40);

    for (auto & el : tasks) {
        el = distribution(generator);
    }
    for (int p = 1; p < processes; ++p) {
        tasks.push_back(empty_task());
    }

    vector<int> proc_to_task_id(processes, -1);
    vector<int> results(N);

    const auto par_start_time = MPI_Wtime();

    int task_id = 0;
    for (auto task : tasks) {

        MPI_Status status;
        int result;
        const int code = MPI_Recv(&result, 1, MPI_INT, MPI_ANY_SOURCE,
                                  RESULT_TAG, MPI_COMM_WORLD, &status);
        if ( code != MPI_SUCCESS ) {
            MPI_Abort(MPI_COMM_WORLD, code);
        }

        const auto sender = status.MPI_SOURCE;
        const auto prev_task_id = proc_to_task_id[sender];
        if (prev_task_id != -1) {
            results[prev_task_id] = result;
        }

        proc_to_task_id[sender] = task_id;
        MPI_Send(&task, 1, MPI_INT, sender, TASK_TAG, MPI_COMM_WORLD);

        ++task_id;
    }
    const auto par_duration = (MPI_Wtime() - par_start_time);

    printf("[Master]: All computations have finished in %.2lf sec.\n",
           par_duration);
    printf("[Master]: Results:\n");
    for (int i = 0; i < N; ++i) {
        printf("%4d  --  %10d\n", tasks[i], results[i]);
    }

    vector<int> verify_results;
    const auto seq_start_time = MPI_Wtime();
    for (int i = 0; i < N; ++i) {
        auto task = tasks[i];
        verify_results.push_back(fibonacci(task));
    }
    const auto seq_duration = (MPI_Wtime() - seq_start_time);
    printf("[Master]: Results for sequential execution:\n");
    for (int i = 0; i < N; ++i) {
        printf("%4d  --  %10d\n", tasks[i], verify_results[i]);
    }
    printf("[Master]: Seq. computations have finished in %.2lf sec.\n",
           seq_duration);
    printf("[Master]: Speedup was: %.1lfx\n", seq_duration / par_duration);
}

void perform_computations(int my_rank) {
    const int Master = 0;
    int result = -1;

    while (true) {
        printf("[%d] sending result -> [%d] to master\n", my_rank, result);

        MPI_Send(&result, 1, MPI_INT, Master, RESULT_TAG, MPI_COMM_WORLD);

        int task;
        MPI_Status status;
        MPI_Recv(&task, 1, MPI_INT, Master, TASK_TAG, MPI_COMM_WORLD, &status);

        printf("[%d] received a task -> [%d] from master\n", my_rank, task);

        if (is_empty_task(task)) {
            break ;
        }
        result = fibonacci(task);
    }
}


int main(int argc, char *argv[]) {
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        cerr << "Cannot initialize MPI subsystem. Aborting.\n";
        abort();
    }

    int processes;
    int rank;
    MPI_Comm_size(MPI_COMM_WORLD, &processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        coordinate_computations(processes);
    } else {
        perform_computations(rank);
    }
    MPI_Finalize();
    return 0;
}
