idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES spiffs nvs_flash ntp_client schedule train esp_driver_gpio esp_driver_gptimer sn74hc595
)

# Create a SPIFFS image from the contents of the 'spiffs_root' directory
# that fits the partition named 'spiffs_storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.

# NOTE: once the partiton is flashed there is no need to flash it again unless
# a change it's made to stop_fixed.csv - decomment FLASH_IN_PROJECT in that case
spiffs_create_partition_image(spiffs_storage ../spiffs_root) # FLASH_IN_PROJECT)