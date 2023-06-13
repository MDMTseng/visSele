#ifdef __APPLE__
    int GrantCameraPermission();
#else
    int GrantCameraPermission(){
        return 0;
    }
#endif