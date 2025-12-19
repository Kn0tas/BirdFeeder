set(MODEL_TFLITE "vision/model_int8.tflite")

idf_component_register(
    SRCS
        "app_main.c"
        "power_manager.c"
        "max17048.c"
        "sensors/pir.c"
        "sensors/camera.c"
        "vision/vision.c"
        "vision/model_data.c"
        "actuators/servo.c"
        "comms/wifi.c"
        "comms/ota.c"
        "storage/fram.c"
        "logging/events.c"
    INCLUDE_DIRS
        "."
        "sensors"
        "actuators"
        "comms"
        "storage"
        "logging"
        "vision"
    EMBED_FILES
        ${MODEL_TFLITE}
)
