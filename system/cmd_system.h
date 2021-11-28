/* Console example — various system commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct {
    struct arg_dbl *timeout;
    struct arg_dbl *interval;
    struct arg_int *data_size;
    struct arg_int *count;
    struct arg_int *tos;
    struct arg_str *host;
    struct arg_end *end;
} ping_args;

// Register all system functions
void register_system(void);

// Register common system functions: "version", "restart", "free", "heap", "tasks"
void register_system_common(void);

// Register deep and light sleep functions
void register_system_sleep(void);

void register_ping(void);

#ifdef __cplusplus
}
#endif
