#import <PlatUtil.hpp>
#import <AVFoundation/AVFoundation.h>

#import <iostream>



int _GrantCameraPermission()
{
    __block int ret = -3;

    AVAuthorizationStatus authorizationStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (authorizationStatus == AVAuthorizationStatusNotDetermined)
    {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
            if (granted)
            {
                NSLog(@"Camera access granted!");
                ret = 0;
            }
            else
            {
                NSLog(@"Camera access denied!");
                ret = -1;
            }
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
    else if (authorizationStatus == AVAuthorizationStatusAuthorized)
    {
        NSLog(@"Camera access already granted!");
        ret = 0;
    }
    else
    {
        NSLog(@"Camera access denied!");
        ret = -2;
    }

    return ret;
}

int GrantCameraPermission(){
    return _GrantCameraPermission();
}