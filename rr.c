#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  u32 remain_t;
  u32 exec_t;
  u32 wait_t;
  u32 response_t;
  u32 end_t;
  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_wait_t = 0;
  u32 total_response_t = 0;

  /* Your code here */
  bool finished = false;
  int processes_finished = 0;
  int t = 0;
  int quantum_left = quantum_length;

  struct process *p;

  //keep running until finished
  while (!finished)
  {
    //loop through arrival times and see which to add
    //first processes come first
    int i = 0;
    while (i < size)
    {
        if(data[i].arrival_time == t)
        {
            p = &data[i];
            p->remain_t = p->burst_time;
            p->exec_t = -1;
            p->wait_t = -1;
            TAILQ_INSERT_TAIL(&list, p, pointers);
        }
        i++;
    }

    //check if queue is not empty
    if (!TAILQ_EMPTY(&list))
    {
        // Check if the quantum has expired
        if (quantum_left == 0)
        {
            // Refresh the quantum
            quantum_left = quantum_length;
            // Retrieve the first process in the queue
            p = TAILQ_FIRST(&list);
            // Remove the process from its current position in the queue
            TAILQ_REMOVE(&list, p, pointers);

            // Determine if the process should be moved to the tail or removed
            if (p->remain_t > 0)
            {
                // Move the process to the tail of the queue if it still needs more time
                TAILQ_INSERT_TAIL(&list, p, pointers);
            }
            else
            {
                // Process has finished execution
                processes_finished++;
                // Optionally log process completion details
                // printf("process %d: end at %d wait %d response %d\n", p->pid, p->end_t, p->wait_t, p->response_t);
            }
        }
    }

    //make sure the remove head didn't empty queue
    if (!TAILQ_EMPTY(&list))
    {
        // Handle the first unit in the queue
        p = TAILQ_FIRST(&list);

        // Initialize execution and response times if not already set
        if (p->exec_t == -1)
        {
            p->exec_t = t;
            p->response_t = t - p->arrival_time;
        }

        // Decrement the remaining time for the current process
        p->remain_t--;
        // Decrement the quantum left for the current time slice
        quantum_left--;

        // Check if the current process is completed
        if (p->remain_t == 0)
        {
            // Set the end time for the process
            p->end_t = t + 1;
            // Calculate the wait time if it has not been set
            if (p->wait_t == -1)
            {
                p->wait_t = t + 1 - p->arrival_time - p->burst_time;
            }
            // Reset quantum_left to ensure the next process gets a full quantum
            quantum_left = 0;
        }
    }

    // Check if all processes have finished execution
    if (processes_finished == size)
    {
        finished = true;
    }

    // Increment time after each iteration
    t++;
  }

  int j = 0;
  while (j < size)
  {
      total_wait_t += data[j].wait_t;
      total_response_t += data[j].response_t;
      j++;
  }
  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_wait_t / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_t / (float)size);

  free(data);
  return 0;
}
