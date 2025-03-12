/******************************************************************************
 * File: aravis_callback_demo.cpp
 *****************************************************************************/
#include <arv.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>

/******************************************************************************
 * Forward declaration of class so our static callback can see it.
 *****************************************************************************/
class AravisCamera;

/******************************************************************************
 * Callback for ArvStream; called on each new buffer.
 *
 * @param stream      Pointer to ArvStream
 * @param type        Event type (we only care about ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE)
 * @param buffer      The ArvBuffer containing image data
 * @param user_data   User-supplied pointer (we pass `this`, i.e., AravisCamera instance)
 *****************************************************************************/
static void streamCallback(ArvStream* stream, ArvStreamCallbackType type, ArvBuffer* buffer, gpointer user_data);

/******************************************************************************
 * AravisCamera class
 *****************************************************************************/
class AravisCamera {
public:
    // Optional: let the user supply a callback to handle frames as cv::Mat
    using FrameCallback = std::function<void(const cv::Mat& mat, uint64_t frame_id)>;

    AravisCamera();
    ~AravisCamera();

    // Discover all GigE Vision devices
    void discoverCameras();

    // Return discovered camera IDs
    std::vector<std::string> getDiscoveredCameraIDs() const;

    // Connect to a camera by its ID (from the discovered list)
    void connectCamera(const std::string& camera_id);

    // Configure the camera (exposure, gain, ROI, etc.)
    void configureCamera();

    // Start streaming: set up callback and push buffers
    void startCapture();

    // Stop streaming
    void stopCapture();

    // Set user callback (called on each new frame)
    void setFrameCallback(FrameCallback cb) { frame_callback_ = std::move(cb); }

private:
    // Internal cleanup
    void cleanup();

    // Internal buffer processing (called from streamCallback)
    void processFrame(ArvBuffer* buffer);

private:
    bool                   initialized_;
    bool                   streaming_;
    std::vector<std::string> discovered_ids_;

    // Aravis objects (wrapped in GObject smart pointers if desired)
    ArvCamera*  camera_;
    ArvDevice*  device_;
    ArvStream*  stream_;

    // Callback that user can set to handle the frames as cv::Mat
    FrameCallback frame_callback_;

    // Buffers we allocate and push to the stream
    static const int NUM_BUFFERS = 16;
    std::vector<void*> buffer_data_;
};

/******************************************************************************
 * Implementation
 *****************************************************************************/

AravisCamera::AravisCamera()
    : initialized_(false)
    , streaming_(false)
    , camera_(nullptr)
    , device_(nullptr)
    , stream_(nullptr)
{
    // Initialize Aravis (safe to call multiple times; internal reference counting)
    arv_g_type_init();
    initialized_ = true;
}

AravisCamera::~AravisCamera()
{
    stopCapture();
    cleanup();
}

// Discover cameras and store their IDs internally
void AravisCamera::discoverCameras()
{
    if (!initialized_) {
        throw std::runtime_error("Aravis not initialized.");
    }

    arv_update_device_list();
    int num_devices = arv_get_n_devices();
    discovered_ids_.clear();
    for (int i = 0; i < num_devices; i++) {
        const char* id = arv_get_device_id(i);
        if (id) {
            discovered_ids_.push_back(id);
        }
    }
    std::cout << "[discoverCameras] Found " << num_devices << " device(s).\n";
}

std::vector<std::string> AravisCamera::getDiscoveredCameraIDs() const
{
    return discovered_ids_;
}

// Connect to a camera by ID
void AravisCamera::connectCamera(const std::string& camera_id)
{
    if (!initialized_) {
        throw std::runtime_error("Aravis not initialized.");
    }

    stopCapture();
    cleanup();

    std::cout << "[connectCamera] Connecting to camera: " << camera_id << std::endl;
    camera_ = arv_camera_new(camera_id.c_str());
    if (!camera_) {
        throw std::runtime_error("Failed to create ArvCamera. Invalid camera ID?");
    }

    // Retrieve device
    device_ = arv_camera_get_device(camera_);
    if (!device_) {
        throw std::runtime_error("Failed to get ArvDevice from camera.");
    }

    std::cout << "[connectCamera] Successfully connected.\n";
}

// Configure camera example
void AravisCamera::configureCamera()
{
    if (!camera_) {
        throw std::runtime_error("No camera connected. Cannot configure.");
    }

    // Stop acquisition in case it's running
    arv_camera_stop_acquisition(camera_);

    // Example: set exposure time to 10000 us = 10 ms
    arv_camera_set_exposure_time(camera_, 10000.0);

    // Example: set gain
    arv_camera_set_gain(camera_, 5.0);

    // (Re)start acquisition will be done in startCapture()
}

void AravisCamera::startCapture()
{
    if (!camera_) {
        throw std::runtime_error("No camera connected.");
    }
    if (streaming_) {
        std::cout << "[startCapture] Already streaming.\n";
        return;
    }

    // Create stream if not already
    if (!stream_) {
        stream_ = arv_camera_create_stream(camera_, nullptr, nullptr);
        if (!stream_) {
            throw std::runtime_error("Failed to create ArvStream.");
        }

        // Connect our static callback
        arv_stream_set_callback(stream_, streamCallback, this);

        // Allocate and queue a number of buffers
        // For "fast as possible," we can choose a large enough buffer to hold an entire frame.
        // Typically you'd query the payload size. We'll just assume 2 MB for example.
        size_t payload_size = arv_camera_get_payload(camera_);
        if (payload_size < 1024) {
            payload_size = 2 * 1024 * 1024; // fallback if camera didn't provide
        }

        buffer_data_.resize(NUM_BUFFERS);
        for (int i = 0; i < NUM_BUFFERS; i++) {
            buffer_data_[i] = g_malloc(payload_size);
            ArvBuffer* arvBuffer = arv_buffer_new(buffer_data_[i], payload_size, nullptr, nullptr);
            arv_stream_push_buffer(stream_, arvBuffer);
        }

        // Optional: automatically set largest possible packet size (for GigE)
        arv_camera_gv_set_packet_size(camera_, 0);
    }

    // Start acquisition
    arv_camera_start_acquisition(camera_);
    streaming_ = true;
    std::cout << "[startCapture] Started streaming.\n";
}

void AravisCamera::stopCapture()
{
    if (camera_ && streaming_) {
        arv_camera_stop_acquisition(camera_);
        streaming_ = false;
        std::cout << "[stopCapture] Stopped streaming.\n";
    }
}

// Cleanup resources
void AravisCamera::cleanup()
{
    if (stream_) {
        // Destroy stream: unref
        g_object_unref(stream_);
        stream_ = nullptr;
    }

    // Free the buffers we allocated
    for (auto ptr : buffer_data_) {
        if (ptr) g_free(ptr);
    }
    buffer_data_.clear();

    if (camera_) {
        g_object_unref(camera_);
        camera_ = nullptr;
    }
    device_ = nullptr;
}

// Called by the static callback to process the buffer
void AravisCamera::processFrame(ArvBuffer* buffer)
{
    if (!buffer) return;

    ArvBufferStatus status = arv_buffer_get_status(buffer);
    if (status != ARV_BUFFER_STATUS_SUCCESS) {
        // Handle various error statuses
        switch (status) {
        case ARV_BUFFER_STATUS_TIMEOUT:
            std::cerr << "[processFrame] TIMEOUT.\n"; break;
        case ARV_BUFFER_STATUS_MISSING_PACKETS:
            std::cerr << "[processFrame] MISSING_PACKETS.\n"; break;
        case ARV_BUFFER_STATUS_OVERFLOW:
            std::cerr << "[processFrame] OVERFLOW.\n"; break;
        case ARV_BUFFER_STATUS_UNSUPPORTED_PIXEL_FORMAT:
            std::cerr << "[processFrame] UNSUPPORTED_PIXEL_FORMAT.\n"; break;
        default:
            std::cerr << "[processFrame] UNKNOWN ERROR.\n"; break;
        }
        return;
    }

    // Extract frame info
    size_t size = 0;
    uint64_t frame_id = arv_buffer_get_frame_id(buffer);
    uint8_t* data = (uint8_t*)arv_buffer_get_data(buffer, &size);
    int width  = arv_buffer_get_image_width(buffer);
    int height = arv_buffer_get_image_height(buffer);

    if (!data || width <= 0 || height <= 0) {
        std::cerr << "[processFrame] Invalid buffer dimensions or data.\n";
        return;
    }

    // For "fast as possible", we directly wrap the data in a cv::Mat without copying.
    // We assume a MONO8 pixel format for simplicity. Adjust as needed.
    // If your camera is color or uses Bayer, you'd convert it accordingly.
    cv::Mat mat(height, width, CV_8UC1, data);

    // If user provided a callback, call it
    if (frame_callback_) {
        frame_callback_(mat, frame_id);
    }
}

/******************************************************************************
 * The static callback for the ArvStream (called in the same thread by Aravis)
 *****************************************************************************/
static void streamCallback(ArvStream* stream, ArvStreamCallbackType type, ArvBuffer* buffer, gpointer user_data)
{
    // We only care about completed buffers
    if (type != ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE) {
        return;
    }
    if (!buffer) return;

    AravisCamera* self = static_cast<AravisCamera*>(user_data);
    self->processFrame(buffer);

    // Re-queue buffer for next capture
    arv_stream_push_buffer(stream, buffer);
}

/******************************************************************************
 * Example usage in main()
 *****************************************************************************/
// Uncomment to compile as a standalone demo.
int main_aravisTestCode(int argc, char** argv)
{
    try {
        AravisCamera camera;

        // 1) Discover
        camera.discoverCameras();
        auto ids = camera.getDiscoveredCameraIDs();
        if (ids.empty()) {
            std::cerr << "No cameras found.\n";
            return 1;
        }

        // 2) Connect to first camera
        camera.connectCamera(ids[0]);

        // 3) Configure
        camera.configureCamera();

        // 4) Set a frame callback
        camera.setFrameCallback([](const cv::Mat &mat, uint64_t frame_id) {
            // For demonstration, just show some info about each frame
            std::cout << "[FrameCallback] Got frame " << frame_id
                      << " size=" << mat.cols << "x" << mat.rows << std::endl;

            // If you want to display the image with OpenCV (MONO8):
            // cv::imshow("Live", mat);
            // cv::waitKey(1);  // Non-blocking
        });

        // 5) Start capture (non-blocking)
        camera.startCapture();

        // Let it run for a few seconds
        std::cout << "Press ENTER to stop...\n";
        std::cin.get();

        // 6) Stop capture & cleanup
        camera.stopCapture();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}

