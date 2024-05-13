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
  u32 remaining_burst_time;
  bool initialized;
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


bool if_finished(struct process *data, u32 size){
  struct process *current_process;
  for (u32 i=0; i<size; i++){
    current_process = &data[i];
    if(current_process->remaining_burst_time != 0)
      return false;
  }
  return true;
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

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */

  u32 current_time = data[0].arrival_time;
  u32 current_quantum = 1;
  bool finished = false;
  struct process *current_process;
  for (u32 i = 0; i<size; i++){
    current_process = &data[i];
    current_process->remaining_burst_time = current_process->burst_time;
    current_process->initialized = false;
    if (current_process->arrival_time < current_time){
      current_time = current_process->arrival_time;
    }
  }

  //  for (u32 i = 0; i < size; i++){
  //   struct process *process = &data[i];
  //   printf("pid: %d, arrival_time: %d, burst_time: %d, remaining_burst_time: %d, initialized: %d\n", process->pid, process->arrival_time, process->burst_time, process->remaining_burst_time, process->initialized);

  // }
  struct process *new_process;

  while(!finished){
    //add new arriving process to the list
    // printf("current_time: %d, current_quantum:%d, finished:%d\n", current_time, current_quantum, finished);
    for (u32 i = 0; i<size; i++){
      new_process = &data[i];
      if (new_process->arrival_time == current_time){
        TAILQ_INSERT_TAIL(&list, new_process, pointers);
      }
    }
    //if current process is done with quantum and is not burst yet

    if (current_process && current_quantum == quantum_length+1 && current_process->remaining_burst_time > 0){
      current_quantum = 1;
      TAILQ_INSERT_TAIL(&list, current_process, pointers);
    }

    //if at the beginning of a new round, get the next process from the list
    if(!current_process && TAILQ_EMPTY(&list)){
      // printf("check empty point: %d\n", current_time);
      current_quantum = 0;
    }
    else if(current_quantum == 1){
      current_process = TAILQ_FIRST(&list);
      TAILQ_REMOVE(&list, current_process, pointers);
    }
    
    // //debug message
    // if(current_process){
    //     printf("current_time: %d, current_quantum:%d, current_process:%d\n", current_time, current_quantum, current_process->pid);
    // }
    // else{
    //     printf("current_time: %d, current_quantum:%d\n", current_time, current_quantum);
    // }

    if(current_process){
      //if current process is not initialized, initialize it
      if (!current_process->initialized){
        current_process->initialized = true;
        total_response_time = total_response_time + current_time - current_process->arrival_time;
      }

      //if we are in the middle of a quantum
      if (current_quantum < (quantum_length+1) && current_process->remaining_burst_time > 0){
        current_process->remaining_burst_time = current_process->remaining_burst_time - 1;
      }

      

      //if current process is done
      if (current_process->remaining_burst_time == 0){
        total_waiting_time = total_waiting_time + current_time - current_process->arrival_time - current_process->burst_time + 1;
        current_quantum = 0;
        TAILQ_REMOVE(&list, current_process, pointers);
        current_process = NULL;
      }
    }


    
    finished = if_finished(data, size);
    current_time++;
    current_quantum++;

    
  }
  


  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
