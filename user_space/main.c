/**
 * @file main.c
 * @brief User-space application program interfacing MIPI CSI camera and
 * captures image.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/videodev2.h>

/**
 * @brief Default text length of a log string.
*/
#define DEFAULT_TEXT_LENGTH 256

/**
 * @brief When an image is captured, it will be saved to this path.
 */
static const char IMAGE_CAPTURE_SAVE_PATH[] =
    "/home/pi/captured_frame_raw.jpeg";

/**
 * @brief Path to the camera device in the dev filesystem. Entry to any V4L2
 * operations.
 */
static const char CAMERA_DEV_PATH[] = "/dev/video0";

/**
 * @brief String buffer to store formatted strings tempoararily for printing.
 */
static char message[DEFAULT_TEXT_LENGTH];

/**
 * @brief Book keeps parameters for image / video capturing.
 * @param device_fs File descriptor to the opened camera hardware, on file
 * system the device is access through CAMERA_DEV_PATH.
 * @param buffer_request Request for frame buffer.
 * @param buffer Video buffer instance.
 * @param buffer_start Start address of the mapped memory of the device.
 * @note The V4L2 structs are defined in <linux/videodev2.h>.
 */
struct camera_params_t {
  int device_fs;
  struct v4l2_format capture_format;
  struct v4l2_requestbuffers buffer_request;
  struct v4l2_buffer buffer;
  void *buffer_start;
};

/**
 * @brief Params for the V4L2 transactions, file static to be shared accross
 * multiple functions.
 */
static struct camera_params_t camera_params;

/**
 * @brief Invoke open system call to open the camera device.
 * @param None.
 * @return None.
 * @note Requires <sys/types.h> <sys/stat.h> <fcntl.h>.
 */
void open_camera_device() {
  /* If successful, stores a nonnegative integer to refer to the opened camera
   * device. */
  camera_params.device_fs = open(CAMERA_DEV_PATH, O_RDWR);

  /* Error out on invalid file descriptor. */
  if (camera_params.device_fs < 0) {
    sprintf(message, "Error opening %s\n", CAMERA_DEV_PATH);
    perror(message);
    exit(1);
  }
}

/**
 * @brief Invoke close system call to close a file descriptor, in this case the
 * camera device.
 * @param None.
 * @return None.
 * @note Requires including <unistd.h>.
 */
void close_camera_device() { close(camera_params.device_fs); }

/**
 * @brief Set the video / image capture_format to be captured by the camera.
 * @param None.
 * @return None.
 * @note ioctl systcall requires <sys/ioctl.h>.
 * The  ioctl()  system  call  manipulates the underlying device parameters of
 * special files.
 */
void set_video_format() {
  int status_code = EXIT_SUCCESS;

  /* Options from enum v4l2_buf_type, select video capture. */
  camera_params.capture_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  /* Configure v4l2_pix_format. */
  camera_params.capture_format.fmt.pix.width = 1920;
  camera_params.capture_format.fmt.pix.height = 1080;
  camera_params.capture_format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  camera_params.capture_format.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;

  /* Latch video capture_format. */
  status_code = ioctl(camera_params.device_fs, VIDIOC_S_FMT,
                      &camera_params.capture_format);

  /* Exit on invalid status. */
  if (status_code < 0) {
    perror("VIDIOC_S_FMT");
    exit(1);
  }
}

/**
 * @brief Request buffer from V4L2.
 * @param None.
 * @return None.
 * @note Memory mapped buffers are located in device memory and must be
 * allocated with this ioctl before they can be mapped into the application’s
 * address space. User buffers are allocated by applications themselves, and
 * this ioctl is merely used to switch the driver into user pointer I/O mode.
 */
void request_buffer() {
  int status_code = EXIT_SUCCESS;

  /* Options from enum enum v4l2_buf_type, select video_capture (take a photo).
   */
  camera_params.buffer_request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  /* Options from enum v4l2_memory, select mmap. */
  camera_params.buffer_request.memory = V4L2_MEMORY_MMAP;
  /* Request one buffer. */
  camera_params.buffer_request.count = 1;

  /* Latch buffer request. */
  status_code = ioctl(camera_params.device_fs, VIDIOC_REQBUFS,
                      &camera_params.buffer_request);

  if (status_code < 0) {
    perror("VIDIOC_REQBUFS");
    exit(1);
  }
}

/**
 * @brief Allocate buffer to store an image frame capture.
 * @param None.
 * @return None.
 */
void allocate_buffer() {
  int status_code;

  memset(&camera_params.buffer, 0, sizeof(camera_params.buffer));

  camera_params.buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  camera_params.buffer.memory = V4L2_MEMORY_MMAP;
  camera_params.buffer.index = 0;

  /* Used to query the status of a buffer. */
  /* Applications set the type field of a struct v4l2_buffer to the same buffer
   * type as was previously used with struct v4l2_format type and struct
   * v4l2_requestbuffers type, and the index field. */
  status_code =
      ioctl(camera_params.device_fs, VIDIOC_QUERYBUF, &camera_params.buffer);

  /* Exit on invalid status. */
  if (status_code < 0) {
    perror("Failure on VIDIOC_QUERYBUF, exiting...\n");
    exit(1);
  }

  /* Maps the /dev/videox camera device file content to a virtual memory
   * address, with start address buffer_start. */
  camera_params.buffer_start =
      mmap(NULL, camera_params.buffer.length, PROT_READ | PROT_WRITE,
           MAP_SHARED, camera_params.device_fs, camera_params.buffer.m.offset);

  /* Exit on invalid status. */
  if (camera_params.buffer_start == MAP_FAILED) {
    perror("Failure on mmap, exiting...\n");
    exit(1);
  }

  /* Make buffer clean state to be ready for an arriving frame. */
  memset(camera_params.buffer_start, 0, camera_params.buffer.length);
}

/**
 * @brief Activate streaming on the camera.
 * @param None.
 * @return None.
 */
void activate_streaming() {
  int status_code;

  /* Clean state. */
  memset(&camera_params.buffer, 0, sizeof(camera_params.buffer));

  camera_params.buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  camera_params.buffer.memory = V4L2_MEMORY_MMAP;
  /* Queueing buffer index 0. */
  camera_params.buffer.index = 0;

  /* Latch streaming on. */
  status_code = ioctl(camera_params.device_fs, VIDIOC_STREAMON,
                      &camera_params.buffer.type);

  if (status_code < 0) {
    perror("VIDIOC_STREAMON");
    exit(1);
  }
}

/**
 * @brief Get a single frame and store to buffer.
 * @param None.
 * @return None.
 */
void get_frame() {
  int status_code;

  /* Queue buffer: submitting the empty buffer in the driver's incoming queue,
   * to fill CMOS captured pixels. */
  status_code =
      ioctl(camera_params.device_fs, VIDIOC_QBUF, &camera_params.buffer);

  if (status_code < 0) {
    perror("VIDIOC_QBUF");
    exit(1);
  }

  /* Dequeue buffer: retrieving the filled data with beautiful pixels. */
  status_code =
      ioctl(camera_params.device_fs, VIDIOC_DQBUF, &camera_params.buffer);

  if (status_code < 0) {
    perror("VIDIOC_QBUF");
    exit(1);
  }
}

/**
 * @brief Deactivate streaming on the camera.
 * @param None.
 * @return None.
 */
void deactivate_streaming() {
  int status_code;

  /* Latch streaming off. */
  status_code = ioctl(camera_params.device_fs, VIDIOC_STREAMOFF,
                      &camera_params.buffer.type);

  /* Exit on error status. */
  if (status_code < 0) {
    perror("VIDIOC_STREAMOFF");
    exit(1);
  }
}

/**
 * @brief Save a captured frame in buffer to as a jpeg image file.
 * @param None.
 * @return None.
 * @note open syscall requires <sys/types.h> <sys/stat.h> <fcntl.h>.
 */
void save_to_image() {
  int image_fd;

  /* Create this file if not exist, write only. */
  image_fd = open(IMAGE_CAPTURE_SAVE_PATH, O_WRONLY | O_CREAT, 0660);

  /* Exit on invalid descriptor. */
  if (image_fd < 0) {
    perror("open");
    exit(1);
  }

  /* Convert the buffer to image. */
  write(image_fd, camera_params.buffer_start, camera_params.buffer.length);

  /* Close the file descriptor. */
  close(image_fd);
}

/**
 * @brief Main routine.
 * @param None.
 * @return None.
 */
int main(void) {

  open_camera_device();

  set_video_format();
  request_buffer();
  allocate_buffer();

  activate_streaming();
  get_frame();
  deactivate_streaming();

  save_to_image();
  close_camera_device();

  printf("Image capture successful, saved to %s\n", IMAGE_CAPTURE_SAVE_PATH);

  return EXIT_SUCCESS;
}
