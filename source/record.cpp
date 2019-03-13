#include <CMDParser.hpp>
#include <FileBuffer.hpp>
#include <ChronoMeter.hpp>
#include <zmq.hpp>

#include <iostream>
#include <string.h>

int main(int argc, char* argv[]){

  unsigned colorsize;
  unsigned depthsize;
  unsigned wait_frames_to_before_start = 0;
  unsigned num_kinect_cameras = 1;
  unsigned num_seconds_to_record = 10;
  bool rgb_is_compressed = false;
  CMDParser p("record_to_this_filename serverport");
  p.addOpt("k",1,"num_kinect_cameras", "specify how many kinect cameras are in stream, default: 1");
  p.addOpt("n",1,"num_seconds_to_record", "specify how many seconds should be recorded, default: 10");
  p.addOpt("c",-1,"rgb_is_compressed", "enable compressed recording for rgb stream, default: false");
  p.addOpt("w",1,"wait_frames_to_before_start", "specify how many seconds to wait before start, default: 0");
  p.addOpt("r",4,"realsense", "enable display for realsense cameras and specify resolution of color and depth sensor e.g. 1280 720 1280 720, default: Kinect V2");
  p.init(argc,argv);

  if(p.isOptSet("w")){
    wait_frames_to_before_start = p.getOptsInt("w")[0];
  }
  if(p.isOptSet("k")){
    num_kinect_cameras = p.getOptsInt("k")[0];
  }
  if(p.isOptSet("n")){
    num_seconds_to_record = p.getOptsInt("n")[0];
  }
  if(p.isOptSet("c")){
    rgb_is_compressed = true;
  }

  if(p.isOptSet("r")){
    unsigned width_dir = p.getOptsInt("r")[2];
    unsigned height_dir = p.getOptsInt("r")[3];
    unsigned width_c = p.getOptsInt("r")[0];
    unsigned height_c = p.getOptsInt("r")[1];

    if(rgb_is_compressed){
        std::cout << "compressed color not supported for the resolution specified. Exiting" << std::endl;
        exit(0);
    }


    colorsize = rgb_is_compressed ? 460800 : width_c * height_c * 3;
    depthsize = width_dir * height_dir * sizeof(float);
    std::cout << "recording for realsense cameras enabled!" << std::endl;
  }
  else{
    colorsize = rgb_is_compressed ? 691200 : 1280 * 1080 * 3;
    depthsize = 512 * 424 * sizeof(float);
  }
  

  FileBuffer fb(p.getArgs()[0].c_str());
  if(!fb.open("w",0 /*20073741824 20 GB buffer*/)){
    std::cerr << "error opening " << p.getArgs()[0] << " exiting..." << std::endl;
    return 1;
  }


  zmq::context_t ctx(1); // means single threaded
  zmq::socket_t  socket(ctx, ZMQ_SUB); // means a subscriber
  socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#if ZMQ_VERSION_MAJOR < 3
  uint64_t hwm = 1;
  socket.setsockopt(ZMQ_HWM,&hwm, sizeof(hwm));
#else
  uint32_t hwm = 1;
  socket.setsockopt(ZMQ_RCVHWM,&hwm, sizeof(hwm));
#endif 
  std::string endpoint("tcp://" + p.getArgs()[1]);
  socket.connect(endpoint.c_str());

  while(wait_frames_to_before_start > 0){

    zmq::message_t zmqm((colorsize + depthsize) * num_kinect_cameras);
    socket.recv(&zmqm); // blocking

    std::cout << "countdown: " << wait_frames_to_before_start << std::endl;

    --wait_frames_to_before_start;
  }

  std::cout << "START!!!!!!!!!!!!!!!!!!!!!!!!!!!!: " << std::endl;

  ChronoMeter cm;
  const double starttime = cm.getTick();
  bool running = true;
  while(running){

    zmq::message_t zmqm((colorsize + depthsize) * num_kinect_cameras);
    socket.recv(&zmqm); // blocking

    const double currtime = cm.getTick();
    const double elapsed = currtime - starttime;
    std::cout << "remaining seconds: " << num_seconds_to_record - elapsed << std::endl;
    if(elapsed > num_seconds_to_record)
      running = false;
    memcpy((char*) zmqm.data(), (const char*) &currtime, sizeof(double));

    unsigned offset = 0;
    for(unsigned i = 0; i < num_kinect_cameras; ++i){
      fb.write((unsigned char*) zmqm.data() + offset, colorsize);
      offset += colorsize;
      fb.write((unsigned char*) zmqm.data() + offset, depthsize);
      offset += depthsize;
    }



  }

  fb.close();

  return 0;
}
