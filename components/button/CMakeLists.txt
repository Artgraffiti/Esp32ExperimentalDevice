set(srcs "button.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_timer esp_event driver)
