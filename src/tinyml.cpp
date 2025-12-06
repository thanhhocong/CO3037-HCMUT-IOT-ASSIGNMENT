#include "task_tinyml.h"
#include "global.h"

// --- Cấu hình TensorFlow Lite ---
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 16 * 1024; 
    uint8_t tensor_arena[kTensorArenaSize];
}

// HÀM QUAN TRỌNG: Định nghĩa ý nghĩa dựa trên Dataset của bạn
const char* getLabelMeaning(int class_index) {
    switch (class_index) {
        case 0: 
            return "LY TUONG (Comfort)"; // Temp ~25, Humi ~50%
        case 1: 
            return "BAT ON (Warning)";   // Lạnh quá hoặc Hơi nóng
        case 2: 
            return "NGUY HIEM (Danger)"; // Temp > 40 hoac Humi > 90%
        default: 
            return "KHONG XAC DINH";
    }
}

void setupTinyML()
{
    Serial.println("[AI-INIT] Dang khoi tao TensorFlow Lite...");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    // Nạp Model đã train từ dataset
    model = tflite::GetModel(env_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Loi phien ban Model!");
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        error_reporter->Report("Loi cap phat bo nho!");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
    Serial.println("[AI-INIT] Khoi tao xong. San sang du doan.");
}

void predict(float *input_data, float *output_data)
{
    if (input == nullptr || interpreter == nullptr) return;

    // 1. Đưa dữ liệu Nhiệt độ, Độ ẩm vào Model
    for (size_t i = 0; i < 2; i++) input->data.f[i] = input_data[i];

    // 2. Chạy suy luận (Model tính toán dựa trên dataset đã học)
    if (interpreter->Invoke() != kTfLiteOk)
    {
        error_reporter->Report("Loi suy luan AI");
        return;
    }

    // 3. Lấy kết quả xác suất (0, 1, 2)
    for (size_t i = 0; i < 3; i++) output_data[i] = output->data.f[i];
}

void tiny_ml_task(void *pvParameters)
{
    Serial.println("[TASK] TinyML Monitor Started...");
    vTaskDelay(pdMS_TO_TICKS(2000)); 

    for (;;)
    {
        // Kiểm tra dữ liệu cảm biến
        if (g_sensorData == NULL) {
             vTaskDelay(pdMS_TO_TICKS(1000));
             continue;
        }

        float t = g_sensorData->temperature;
        float h = g_sensorData->humidity;

        // Bỏ qua giá trị rác
        if (isnan(t) || isnan(h)) {
             vTaskDelay(pdMS_TO_TICKS(1000));
             continue;
        }

        // --- BẮT ĐẦU DỰ ĐOÁN ---
        float input_values[2] = {t, h};
        float prediction[3] = {0};
        
        predict(input_values, prediction);

        // Tìm nhãn có xác suất cao nhất
        int predicted_class = 0;
        float max_prob = prediction[0];
        
        // So sánh xác suất các lớp 0, 1, 2
        for (int i = 1; i < 3; i++)
        {
            if (prediction[i] > max_prob)
            {
                max_prob = prediction[i];
                predicted_class = i;
            }
        }

        // --- HIỂN THỊ KẾT QUẢ THEO NGUYÊN LÝ DATASET ---
        // In ra màn hình Terminal
        Serial.printf("--------------------------------------------------\n");
        Serial.printf("[SENSOR] Temp: %.1f C  |  Humi: %.1f %%\n", t, h);
        
        // Hiển thị thanh mức độ tin cậy của AI
        Serial.printf("[AI AI]  Du doan: %s\n", getLabelMeaning(predicted_class));
        Serial.printf("[PROB]   Do tin cay: %.1f %%\n", max_prob * 100);
        
        // Giải thích thêm cho người dùng dễ hiểu tại sao AI chọn vậy
        if (predicted_class == 2) {
             Serial.println("=> CANH BAO: Moi truong khac nghiet (Nong/Am qua muc)!");
        } else if (predicted_class == 0) {
             Serial.println("=> OK: Moi truong thoai mai.");
        }

        Serial.printf("--------------------------------------------------\n\n");

        // Delay 3 giây để dễ quan sát
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}