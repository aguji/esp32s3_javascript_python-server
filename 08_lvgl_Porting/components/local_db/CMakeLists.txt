idf_component_register(
    SRCS 
        "local_db.c"
        "local_db_data.c"
    INCLUDE_DIRS "."
    REQUIRES esp_common
)
