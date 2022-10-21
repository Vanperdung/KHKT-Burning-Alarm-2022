#ifndef _SPIFFS_USER_H_
#define _SPIFFS_USER_H_

esp_err_t write_to_file(char *file_name, char *buf);
esp_err_t read_from_file(char *file_name, char *buf);
void mount_SPIFFS(void);
void remove_file(char *file_name);

#endif