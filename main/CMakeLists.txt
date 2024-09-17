idf_component_register(SRCS "opencv_orb.cpp" "main.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage spiffs-image FLASH_IN_PROJECT)