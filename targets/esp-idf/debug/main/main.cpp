#include <stdio.h>
#include <thread>
#include <chrono>

int global_counter = 0;
float global_temperature = 25.5f;
const char* global_message = "ESP32-C6 Debug Example";

extern "C" void app_main(void)
{
  int loop_count = 0;
  bool is_running = true;
  double elapsed_time = 0.0;

  printf("Starting %s\n", global_message);
  printf("Initial temperature: %.1f°C\n", global_temperature);

  while (is_running) {
      global_counter++;
      loop_count++;
      elapsed_time = loop_count * 1.0;
      global_temperature += 0.1f;

      printf("Loop #%d | Global Counter: %d | Temperature: %.2f°C | Elapsed: %.1fs\n",
             loop_count, global_counter, global_temperature, elapsed_time);

      if (loop_count % 10 == 0) {
          printf(">>> Checkpoint: 10 iterations completed!\n");
      }

      std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
