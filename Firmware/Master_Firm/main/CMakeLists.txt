idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "spiffs_user.c" "button.c" "led.c" "mqtt.c" "sim.c" "fota.c" "smartconfig.c" "lora.c" "wifi.c" "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES ${project_dir}/certs/git_ota.pem)