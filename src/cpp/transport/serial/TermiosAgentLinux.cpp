// Copyright 2017-present Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <uxr/agent/transport/serial/TermiosAgentLinux.hpp>

#include <fcntl.h>
#include <unistd.h>

namespace eprosima {
namespace uxr {

TermiosAgent::TermiosAgent(
        char const* dev,
        int open_flags,
        termios const& termios_attrs,
        uint8_t addr,
        Middleware::Kind middleware_kind)
    : SerialAgent(addr, middleware_kind)
    , dev_{dev}
    , open_flags_{open_flags}
    , termios_attrs_{termios_attrs}
{
}

TermiosAgent::~TermiosAgent()
{
    try
    {
        stop();
    }
    catch (std::exception& e)
    {
        UXR_AGENT_LOG_CRITICAL(
            UXR_DECORATE_RED("error stopping server"),
            "exception: {}",
            e.what());
    }
}


/*******************************************************************************
*
* @brief        This function goal is to send a shutdown package
*               to the micro-ROS client in order to stop it.
*
* @param	fd: the file descriptor for where to send the message
*
* @return	void
*
* @note		None.
*
*******************************************************************************/
void TermiosAgent::send_shutdown(int filedescriptor)
{
  ssize_t bytes_sent = -1;
  unsigned int umsg[8] = {
      SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG,
      SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG
  };

  bytes_sent = ::write(filedescriptor, &umsg, sizeof(umsg));
  if (0 >= bytes_sent){
    UXR_ERROR("Failed to write SHUTDOWN_MSG", strerror(errno));
  }
}

/*******************************************************************************
*
* @brief        Creates the rpmsg endpoint
*
* @param	rpfd:
*
* @param	eptinfo:
*
* @return	true if the creation succedeed. False else.
*
* @note		None.
*
*******************************************************************************/
int TermiosAgent::rpmsg_create_ept(int rpfd, rpmsg_endpoint_info *ept)
{
  int ret;

  if (0 <= rpfd && NULL != ept) {
    ret = ioctl(rpfd, RPMSG_CREATE_EPT_IOCTL, ept);
    UXR_PRINTF("tried ioctl, got a ret value", ret);
  } else {
    UXR_ERROR("Some null valued was received", strerror(errno));
    ret = -1;
  }

  if (-1 == ret) {
    UXR_ERROR("Failed to write to rpfd", strerror(errno));
  }
  return ret;
}

/*******************************************************************************
*
* @brief        Looks for the endpoint's device name
*
* @param	rpmsg_char_name:
*
* @param	ept_name:
*
* @param	ept_dev_name:
*
* @return       a string with the endpoint name if found
*
* @note		None.
*
*******************************************************************************/
char *TermiosAgent::get_rpmsg_ept_dev_name(const char *rpmsg_name,
				    const char *ept_name,
				    char *ept_device_name)
{
  char sys_rpmsg_ept_name_path[64];
  char svc_name[64];
  const char *sys_rpmsg_path = "/sys/class/rpmsg";
  FILE *fp;
  int i;
  long unsigned int ept_name_len;

  for (i = 0; i < 128; i++) {
    sprintf(sys_rpmsg_ept_name_path, "%s/%s/rpmsg%d/name",
	    sys_rpmsg_path, rpmsg_name, i);
    UXR_PRINTF("checking sys_rpmsg_ept_name_path", sys_rpmsg_ept_name_path);
    if (access(sys_rpmsg_ept_name_path, F_OK) < 0)
      continue;
    fp = fopen(sys_rpmsg_ept_name_path, "r");
    if (!fp) {
      UXR_ERROR("failed to open sys rpmsg path", sys_rpmsg_ept_name_path);
      UXR_ERROR("errno is", strerror(errno));
      break;
    }
    if (fgets(svc_name, sizeof(svc_name), fp) == NULL){
      UXR_ERROR("Unable to read the file pointer. Exiting.", strerror(errno));
      return NULL;
    }
    fclose(fp);
    UXR_PRINTF("svc_name", svc_name);
    ept_name_len = strlen(ept_name);
    if (ept_name_len > sizeof(svc_name))
      ept_name_len = sizeof(svc_name);
    if (!strncmp(svc_name, ept_name, ept_name_len)) {
      sprintf(ept_device_name, "rpmsg%d", i);
      return ept_device_name;
    }
  }

  UXR_ERROR("Not able to RPMsg endpoint file", rpmsg_name);
  UXR_ERROR("Faild endpoint is", ept_name);
  return NULL;
}

/*******************************************************************************
*
* @brief        Binds the rpmsg device to rpmsg char driver
*
* @param	rpmsg_dev_name:
*
* @return	0 as a int if OK, negative error code else
*
* @note		None.
*
*******************************************************************************/
int TermiosAgent::bind_rpmsg_chrdev(const char *rpmsg_name)
{
  char fpath_chrdev[256];
  const char *rpmsg_chdrv = "rpmsg_chrdev";
  int fd_chrdev;
  int ret;

  /* rpmsg dev overrides path */
  sprintf(fpath_chrdev, "%s/devices/%s/driver_override",
	  RPMSG_BUS_SYS, rpmsg_name);
  UXR_PRINTF("open fpath_chrdev", fpath_chrdev);
  fd_chrdev = open(fpath_chrdev, O_WRONLY);
  if (fd_chrdev < 0) {
    UXR_ERROR("Failed to open fpath_chrdev", fpath_chrdev);
    UXR_ERROR("errno is", strerror(errno));
    return -EINVAL;
  }
  ret = ::write(fd_chrdev, rpmsg_chdrv, strlen(rpmsg_chdrv) + 1);
  if (ret < 0) {
    UXR_ERROR("Failed to write rpmsg_chdrv", rpmsg_chdrv);
    UXR_ERROR("chdrv failed path", fpath_chrdev);
    UXR_ERROR("errno is", strerror(errno));
    return -EINVAL;
  }
  close(fd_chrdev);

  /* bind the rpmsg device to rpmsg char driver */
  sprintf(fpath_chrdev, "%s/drivers/%s/bind", RPMSG_BUS_SYS, rpmsg_chdrv);
  fd_chrdev = open(fpath_chrdev, O_WRONLY);
  if (fd_chrdev < 0) {
    UXR_ERROR("Failed to open fpath_chrdev", fpath_chrdev);
    UXR_ERROR("errno is", strerror(errno));
    return -EINVAL;
  }
  UXR_PRINTF("write rpmsg dev", rpmsg_name);
  UXR_PRINTF("write to path", fpath_chrdev);
  ret = ::write(fd_chrdev, rpmsg_name, strlen(rpmsg_name) + 1);
  if (ret < 0) {
    UXR_ERROR("Failed to write rpmsg dev in path", rpmsg_name);
    UXR_ERROR("failed path", fpath_chrdev);
    UXR_ERROR("errno is", strerror(errno));
    return -EINVAL;
  }
  close(fd_chrdev);
  return 0;
}

/****************************************************************************
*
* @brief        Looks for the rpmsg character device file descriptor
*
* @param	rpmsg_name:
*
* @param	rpmsg_ctrl_name:
*
* @return	true if the creation succedeed. False else.
*
* @note		None.
*
*****************************************************************************/
int TermiosAgent::get_rpmsg_chrdev_fd(const char *rpmsg_name,
			       char *rpmsg_ctrl_name)
{
  char dpath[2*NAME_MAX];
  DIR *dir;
  struct dirent *ent;
  int chr_fd;

  sprintf(dpath, "%s/devices/%s/rpmsg", RPMSG_BUS_SYS, rpmsg_name);
  UXR_PRINTF("opendir {}", dpath);
  dir = opendir(dpath);
  if (dir == NULL) {
    UXR_ERROR("failed to open dir", dpath);
    UXR_ERROR("errno is", strerror(errno));
    return -EINVAL;
  }
  while ((ent = readdir(dir)) != NULL) {
    if (!strncmp(ent->d_name, "rpmsg_ctrl", 10)) {
      sprintf(dpath, "/dev/%s", ent->d_name);
      closedir(dir);
      UXR_PRINTF("opening path", dpath);
      chr_fd = open(dpath, O_RDWR | O_NONBLOCK);
      if (chr_fd < 0) {
	UXR_ERROR("failed opening fd", dpath);
	UXR_ERROR("errno is", strerror(errno));
	return chr_fd;
      }
      sprintf(rpmsg_ctrl_name, "%s", ent->d_name);
      return chr_fd;
    }
  }

  UXR_ERROR("No rpmsg_ctrl file found in dpath", dpath);
  closedir(dir);
  return -EINVAL;
}

/****************************************************************************
*
* @brief        Sets the source destination
*
* @param	out:
*
* @param	pep:
*
* @return	void
*
* @note		None.
*
*****************************************************************************/
void TermiosAgent::set_src_dst(char *out, rpmsg_endpoint_info *pep)
{
  long dst = 0;
  char *lastdot = strrchr(out, '.');

  if (lastdot == NULL)
    return;
  dst = strtol(lastdot + 1, NULL, 10);
  if ((errno == ERANGE && (dst == LONG_MAX || dst == LONG_MIN))
      || (errno != 0 && dst == 0)) {
    UXR_ERROR("Problem with the setup of the srs dest. errno", strerror(errno));
    return;
  }
  pep->dst = (unsigned int)dst;
}

/****************************************************************************
*
* @brief        Looks for the channel name
*
* @param	out:
*
* @param	pep:
*
* @return	the first dirent matching rpmsg-openamp-demo-channel
*               in /sys/bus/rpmsg/devices/ E.g.:
*	        virtio0.rpmsg-openamp-demo-channel.-1.1024
*
* @note		None.
*
*****************************************************************************/
void TermiosAgent::lookup_channel(char *out, rpmsg_endpoint_info *pep)
{
	char dpath[] = RPMSG_BUS_SYS "/devices";
	struct dirent *ent;
	DIR *dir = opendir(dpath);

	if (dir == NULL) {
	  UXR_ERROR("failed to opendir", dpath);
	  UXR_ERROR("errno is", strerror(errno));
	  return;
	}

	UXR_PRINTF("1", NULL);
	while ((ent = readdir(dir)) != NULL) {
	        UXR_PRINTF("2", NULL);
	        // UXR_PRINTF("check names for ent", ent->d_name);
		// UXR_PRINTF("and for pep", pep->name);
		if (strstr(ent->d_name, pep->name)) {
		  // UXR_PRINTF("strcpy to", out);
		  // UXR_PRINTF("strcpy from", ent->d_name);
		  UXR_PRINTF("3", NULL);
		  strncpy(out, ent->d_name, NAME_MAX+1);
		  // UXR_PRINTF("set src dest", NULL);
		  UXR_PRINTF("4", NULL);
		  set_src_dst(out, pep);
		  // UXR_PRINTF("using dev file", out);
		  UXR_PRINTF("5", NULL);
		  closedir(dir);
		  return;
		}
	}
	closedir(dir);
	UXR_WARNING("Seaching in dpath", dpath);
	UXR_WARNING("No dev file for", pep->name);
}


bool TermiosAgent::init()
{
//     bool rv = false;

//     // Check if serial port exist
//     std::chrono::steady_clock::time_point begin;
//     int serial_exist = 0;
//     int error_count = 0;

//     do
//     {
//         if (serial_exist != 0)
//         {
//             std::this_thread::sleep_for((std::chrono::milliseconds) 10);

//             if (EACCES == errno || EBUSY == errno)
//             {
//                 // Increase error count
//                 error_count++;

//                 if (error_count > 10)
//                 {
//                     // Resource busy or superuser privileges required
//                     break;
//                 }
//             }
//             else if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count())
//             {
//                 begin = std::chrono::steady_clock::now();
//                 UXR_AGENT_LOG_INFO(
//                     UXR_DECORATE_YELLOW("Serial port not found."),
//                     "device: {}, error {}, waiting for connection...",
//                     dev_, errno);
//             }
//         }

//         serial_exist = access(dev_.c_str(), W_OK );
//     }
//     while (serial_exist != 0);

//     poll_fd_.fd = open(dev_.c_str(), open_flags_);
//     if (0 < poll_fd_.fd)
//     {
//         struct termios new_attrs;
//         memset(&new_attrs, 0, sizeof(new_attrs));
//         if (0 == tcgetattr(poll_fd_.fd, &new_attrs))
//         {
//             new_attrs.c_cflag = termios_attrs_.c_cflag;
//             new_attrs.c_lflag = termios_attrs_.c_lflag;
//             new_attrs.c_iflag = termios_attrs_.c_iflag;
//             new_attrs.c_oflag = termios_attrs_.c_oflag;
//             new_attrs.c_cc[VMIN] = termios_attrs_.c_cc[VMIN];
//             new_attrs.c_cc[VTIME] = termios_attrs_.c_cc[VTIME];

// #if _HAVE_STRUCT_TERMIOS_C_ISPEED
//             cfsetispeed(&new_attrs, termios_attrs_.c_ispeed);
// #endif
// #if _HAVE_STRUCT_TERMIOS_C_OSPEED
//             cfsetospeed(&new_attrs, termios_attrs_.c_ospeed);
// #endif

//             if (0 == tcsetattr(poll_fd_.fd, TCSANOW, &new_attrs))
//             {
//                 rv = true;
//                 poll_fd_.events = POLLIN;

//                 tcflush(poll_fd_.fd, TCIOFLUSH);

//                 UXR_AGENT_LOG_INFO(
//                     UXR_DECORATE_GREEN("running..."),
//                     "fd: {}",
//                     poll_fd_.fd);
//             }
//             else
//             {
//                 UXR_AGENT_LOG_ERROR(
//                     UXR_DECORATE_RED("set termios attributes error"),
//                     "errno: {}",
//                     errno);
//             }
//         }
//         else
//         {
//             UXR_AGENT_LOG_ERROR(
//                 UXR_DECORATE_RED("get termios attributes error"),
//                 "errno: {}",
//                 errno);
//         }
//     }
//     else
//     {
//         UXR_AGENT_LOG_ERROR(
//             UXR_DECORATE_RED("open device error"),
//             "device: {}, errno: {}{}",
//             dev_, errno,
//             (EACCES == errno) ? ". Please re-run with superuser privileges." : "");
//     }
//     return rv;


    UXR_PRINTF("RPMsg XRCE-DDS INIT", NULL);

    int ret;
    unsigned char hello[10] = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    // Init the endpoint structure that exists at the class level
    // eptinfo.name = "rpmsg-openamp-demo-channel";
    // eptinfo.src = 0;
    // eptinfo.dst = 0;
    // rpmsg_dev = "virtio0.rpmsg-openamp-demo-channel.-1.0";
    strcpy(rpmsg_dev,"virtio0.rpmsg-openamp-demo-channel.-1.0");

    ret = system("set -x; modprobe rpmsg_char");
    // ret = system("set -x; lsmod; modprobe rpmsg_char");
    if (ret < 0)
      {
	UXR_ERROR("Failed to load rpmsg_char driver.", ret);
	return false;
      }

    UXR_PRINTF("Calling lookup cha func", NULL);
    lookup_channel(rpmsg_dev, &eptinfo);

    // while ((opt = getopt(argc, argv, "d:n:s:e:")) != -1) {
    //   switch (opt) {
    //   case 'd':
    // 	strncpy(rpmsg_dev, optarg, sizeof(rpmsg_dev));
    // 	break;
    //   case 'n':
    // 	ntimes = atoi(optarg);
    // 	break;
    //   case 's':
    // 	eptinfo.src = atoi(optarg);
    // 	break;
    //   case 'e':
    // 	eptinfo.dst = atoi(optarg);
    // 	break;
    //   default:
    // 	UXR_ERROR("getopt return unsupported option",opt);
    // 	break;
    //   }
    // }

    sprintf(fpath, RPMSG_BUS_SYS "/devices/%s", rpmsg_dev);
    if (access(fpath, F_OK))
      {
	UXR_ERROR("access failed for fpath", fpath);
	UXR_ERROR("errno is", strerror(errno));
	return false;
      }
    ret = bind_rpmsg_chrdev(rpmsg_dev);
    if (0 > ret)
      {
	UXR_ERROR("failed to bind chrdev", strerror(errno));
	return false;
      }
    charfd = get_rpmsg_chrdev_fd(rpmsg_dev, rpmsg_char_name);
    if (0 >= charfd)
      {
	UXR_ERROR("obtained charfd ", strerror(errno));
	return false;
      }

    /* Create endpoint from rpmsg char driver */
    UXR_PRINTF("rpmsg_create_ept: name", eptinfo.name);
    UXR_PRINTF("rpmsg_create_ept: source", eptinfo.src);
    UXR_PRINTF("rpmsg_create_ept: destination", eptinfo.dst);
    UXR_PRINTF("rpmsg_create_ept: charfd", charfd);
    ret = rpmsg_create_ept(charfd, &eptinfo);
    if (ret) {
      UXR_ERROR("rpmsg_create_ept failed with error", strerror(errno));
      return false;
    }
    if (!get_rpmsg_ept_dev_name(rpmsg_char_name, eptinfo.name, ept_dev_name))
      return false;
    sprintf(ept_dev_path, "/dev/%s", ept_dev_name);

    UXR_PRINTF("open endpoint path", ept_dev_path);
    poll_fd_.fd = open(ept_dev_path, O_RDWR | O_NONBLOCK);
    if ( 0 >= poll_fd_.fd )
      {
	UXR_ERROR("Unable to open the endpoint. Exit.", strerror(errno));
	perror(ept_dev_path);
	close(charfd);
	return false;
      }


    // i_payload = (struct _payload *)malloc(2 * sizeof(unsigned long) + PAYLOAD_MAX_SIZE);
    // r_payload = (struct _payload *)malloc(2 * sizeof(unsigned long) + PAYLOAD_MAX_SIZE);

    // if (i_payload == 0 || r_payload == 0) {
    //   UXR_ERROR("Failed to allocate memory for payload.", strerror(errno));
    //   return false;
    // }

    UXR_PRINTF("Sending a first message to the remoteproc to start it.", NULL);
    ret = ::write(poll_fd_.fd, &hello, 10);
    if ( 0  >= ret )
      {
	UXR_ERROR("Unable to send data despite EP opening.", strerror(errno));
	return false;
      }
    UXR_PRINTF("RPMsg init is successful.", NULL);

    UXR_PRINTF("GPIO being init.", NULL);
    GPIO_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (GPIO_fd <= 0)
      {
	std::cout << "open error" << std::endl;
	return -1;
      }

    int gpio = static_cast<GPIO_t*>(mmap(nullptr, gpio_size, PROT_READ|PROT_WRITE, MAP_SHARED, GPIO_fd, gpio_base));
    if (!gpio)
      {
	std::cout << "mmap error" << std::endl;
	close(fd);
	return -1;
      }

    UXR_PRINTF("GPIO init is successful.", NULL);
    return true;
}

bool TermiosAgent::fini()
{
  // if (-1 == poll_fd_.fd)
  //   {
  //     return true;
  //   }

  // bool rv = false;
  // if (0 == ::close(poll_fd_.fd))
  //   {
  //     UXR_AGENT_LOG_INFO(
  // 			 UXR_DECORATE_GREEN("server stopped"),
  // 			 "fd: {}",
  // 			 poll_fd_.fd);
  //     rv = true;
  //   }
  // else
  //   {
  //     UXR_AGENT_LOG_ERROR(
  // 			  UXR_DECORATE_RED("close server error"),
  // 			  "fd: {}, errno: {}",
  // 			  poll_fd_.fd, errno);
  //   }

  // poll_fd_.fd = -1;
  // return rv;

  UXR_PRINTF("Custom RPMSg Micro XRCE-DDS Agent FINI function.", NULL);


  if (-1 == poll_fd_.fd)
    return true;

  send_shutdown(poll_fd_.fd);

  if (0 != close(poll_fd_.fd))
    {
      UXR_ERROR("Unable to close the fd", strerror(errno));
      return false;
    }

  if (0 <= charfd)
    {
      if (0 != close(charfd))
	{
	  UXR_ERROR("Unable to close the charfd", strerror(errno));
	  return false;
	}
    }

  UXR_PRINTF("Everything was freed and close cleanly. Returning true.", NULL);
  return true;
}

bool TermiosAgent::handle_error(
        TransportRc /*transport_rc*/)
{
    return fini() && init();
}

} // namespace uxr
} // namespace eprosima
