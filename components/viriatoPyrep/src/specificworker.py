#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
#    Copyright (C) 2020 by YOUR NAME HERE
#
#    This file is part of RoboComp
#
#    RoboComp is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    RoboComp is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with RoboComp.  If not, see <http://www.gnu.org/licenses/>.
#

from genericworker import *
import os, time, queue
from bisect import bisect_left
from os.path import dirname, join, abspath
from pyrep import PyRep
#from pyrep.robots.mobiles.viriato import Viriato
from pyrep.robots.mobiles.viriato import Viriato
from pyrep.robots.mobiles.youbot import YouBot
from pyrep.objects.vision_sensor import VisionSensor
from pyrep.objects.dummy import Dummy
from pyrep.objects.shape import Shape
from pyrep.objects.shape import Object

import numpy as np
import numpy_indexed as npi
from itertools import zip_longest
import cv2
import queue

class SpecificWorker(GenericWorker):
    def __init__(self, proxy_map):
        super(SpecificWorker, self).__init__(proxy_map)
       
    def __del__(self):
        print('SpecificWorker destructor')

    def setParams(self, params):
        
        SCENE_FILE = '../../etc/autonomy-lab.ttt'
        SCENE_FILE = '../../etc/youbot.ttt'
        #SCENE_FILE = '/home/pbustos/software/PyRep/examples/scene_youbot_navigation.ttt'

        self.pr = PyRep()
        self.pr.launch(SCENE_FILE, headless=False)
        self.pr.start()
        
        #self.robot = Viriato()
        self.robot = YouBot()
        self.robot_object = Object("youBot")

        self.cameras = {}
        # cam = VisionSensor("camera_1_rgbd_sensor")
        # self.cameras["camera_1_rgbd_sensor"] = {    "handle": cam, 
        #                                             "id": 1,
        #                                             "angle": np.radians(cam.get_perspective_angle()), 
        #                                             "width": cam.get_resolution()[0],
        #                                             "height": cam.get_resolution()[1],
        #                                             "depth": 3,
        #                                             "focal": cam.get_resolution()[0]/np.tan(np.radians(cam.get_perspective_angle())), 
        #                                             "rgb": np.array(0), 
        #                                             "depth": np.ndarray(0) }
        # cam = VisionSensor("camera_2_rgbd_sensor")                                            
        # self.cameras["camera_2_rgbd_sensor"] = {    "handle": cam, 
        #                                             "id": 2,
        #                                             "angle": np.radians(cam.get_perspective_angle()), 
        #                                             "width": cam.get_resolution()[0],
        #                                             "height": cam.get_resolution()[1],
        #                                             "focal": cam.get_resolution()[0]/np.tan(np.radians(cam.get_perspective_angle())), 
        #                                             "rgb": np.array(0), 
        #                                             "depth": np.ndarray(0) }
        # cam = VisionSensor("camera_3_rgbd_sensor")                                            
        # self.cameras["camera_3_rgbd_sensor"] = {    "handle": cam, 
        #                                             "id": 3,
        #                                             "angle": np.radians(cam.get_perspective_angle()), 
        #                                             "width": cam.get_resolution()[0],
        #                                             "height": cam.get_resolution()[1],
        #                                             "focal": cam.get_resolution()[0]/np.tan(np.radians(cam.get_perspective_angle())), 
        #                                             "rgb": np.array(0), 
        #                                             "depth": np.ndarray(0) }
        # 
        cam = VisionSensor("Viriato_head_camera_front_sensor")
        self.cameras["Viriato_head_camera_front_sensor"] = {    "handle": cam,
                                                                "id": 0,
                                                                "angle": np.radians(cam.get_perspective_angle()),
                                                                "width": cam.get_resolution()[0],
                                                                "height": cam.get_resolution()[1],
                                                                "focal": cam.get_resolution()[0]/np.tan(np.radians(cam.get_perspective_angle())),
                                                                "rgb": np.array(0),
                                                                "depth": np.ndarray(0) }


        self.hokuyo_base_front_left = VisionSensor("hokuyo_base_front_left")
        self.hokuyo_base_front_right = VisionSensor("hokuyo_base_front_right")
        self.hokuyo_base_back_right = VisionSensor("hokuyo_base_back_right")
        self.hokuyo_base_back_left = VisionSensor("hokuyo_base_back_left")

        # 
        # self.people = {}
        # for i in range(1,5):
        #     name = "Bill#" + str(i)
        #     if Dummy.exists(name):
        #         self.people["name"] = Dummy(name)

        self.joystick_newdata = []
        self.speed_robot = []
        self.speed_robot_ant = []

    #@QtCore.Slot()
    def compute(self):
        while True:
        #     try:
        #         #start = time.time()
            self.pr.step()
            for name,cam in self.cameras.items():
                cam = self.cameras["Viriato_head_camera_front_sensor"]
                image_float = cam["handle"].capture_rgb()
                depth = cam["handle"].capture_depth()
                image = cv2.normalize(src=image_float, dst=None, alpha=0, beta=255, norm_type=cv2.NORM_MINMAX, dtype=cv2.CV_8U)
                cam["rgb"] = RoboCompCameraRGBDSimple.TImage(cameraID=cam["id"], width=cam["width"], height=cam["height"], depth=3, focalx=cam["focal"], focaly=cam["focal"], alivetime=time.time(), image=image.tobytes())
                cam["depth"] = RoboCompCameraRGBDSimple.TDepth(cameraID=cam["id"], width=cam["width"], height=cam["height"], focalx=cam["focal"], focaly=cam["focal"], alivetime=time.time(), depth=depth.tobytes())

                try:
                    self.camerargbdsimplepub_proxy.pushRGBD( cam["rgb"],  cam["depth"])
                except Ice.Exception as e:
                    print(e)

                # get People position
        #         people_data = RoboCompHumanToDSRPub.PeopleData()
        #         people_data.timestamp = time.time()
        #         people = [] #RoboCompHumanToDSRPub.People()
        #         for name, handle in self.people.items():
        #             pos = handle.get_position()
        #             rot = handle.get_orientation()
        #             person = RoboCompHumanToDSRPub.Person(0, -pos[1]*1000, pos[2]*1000, pos[0]*1000, -rot[2], {})
        #             people.append(person)
        #         try:
        #             people_data.peoplelist = people
        #             self.humantodsrpub_proxy.newPeopleData(people_data)
        #         except Ice.Exception as e:
        #             print(e)
        # 
        #         # compute TLaserData and publish

            ldata = self.compute_omni_laser([self.hokuyo_base_front_right,
                                              self.hokuyo_base_front_left,
                                              self.hokuyo_base_back_left,
                                              self.hokuyo_base_back_right
                                             ], self.robot)
            try:
                self.laserpub_proxy.pushLaserData(ldata)
            except Ice.Exception as e:
                print(e)
        #         
            # Move robot from data in joystick buffer
            if self.joystick_newdata and (time.time() - self.joystick_newdata[1]) > 0.1:
                self.update_joystick(self.joystick_newdata[0])
                self.joystick_newdata = None
        # 
            # Get and publish robot pose
            pose = self.robot.get_2d_pose()
            linear_vel, ang_vel = self.robot_object.get_velocity()
            #print("Veld:", linear_vel, ang_vel)
            try:
                isMoving = np.abs(linear_vel[1]) > 0.01 or np.abs(linear_vel[1]) > 0.01 or np.abs(ang_vel[2]) > 0.01
                self.bState = RoboCompGenericBase.TBaseState(x=-pose[1]*1000,
                                                             z=pose[0]*1000,
                                                             alpha=-pose[2]-1.5707963,
                                                             advVx=-linear_vel[1]*1000,
                                                             advVz=linear_vel[0]*1000,
                                                             rotV=ang_vel[2],
                                                             isMoving=isMoving)
                self.omnirobotpub_proxy.pushBaseState(self.bState)
            except Ice.Exception as e:
                print(e)
        # 
            # Move robot from data in setSpeedBase
            if self.speed_robot != self.speed_robot_ant:#or (isMoving and self.speed_robot == [0,0,0]):
                self.robot.set_base_angular_velocites(self.speed_robot)
                print("Velocities sent to robot:", self.speed_robot)
                self.speed_robot_ant = self.speed_robot

            time.sleep(0.08)
        #         #print(time.time()-start)
        #     except KeyboardInterrupt:
        #         break

    ###################################################################################################

    # General laser computation
    def compute_omni_laser(self, lasers, robot):
        c_data = []
        coor = []
        for laser in lasers:
            semiwidth = laser.get_resolution()[0]/2
            semiangle = np.radians(laser.get_perspective_angle()/2)
            focal = semiwidth/np.tan(semiangle)
            data = laser.capture_depth(in_meters=True)
            m = laser.get_matrix(robot)     # these data should be read first
            imat = np.array([[m[0],m[1],m[2],m[3]],[m[4],m[5],m[6],m[7]],[m[8],m[9],m[10],m[11]],[0,0,0,1]])

            for i,d in enumerate(data.T):
                z = d[0]        # min if more than one row in depth image
                vec = np.array([-(i-semiwidth)*z/focal, 0, z, 1])
                res = imat.dot(vec)[:3]       # translate to robot's origin, homogeneous
                c_data.append([np.arctan2(res[0], res[1]), np.linalg.norm(res)])  # add to list in polar coordinates

        # create 360 polar rep
        c_data_np = np.asarray(c_data)
        angles = np.linspace(-np.pi, np.pi, 360)                          # create regular angular values
        positions = np.searchsorted(angles, c_data_np[:,0])               # list of closest position for each laser meas
        ldata = [RoboCompLaser.TData(a, 0) for a in angles]               # create empty 360 angle array
        pos , medians  = npi.group_by(positions).median(c_data_np[:,1])   # group by repeated positions
        for p, m in zip_longest(pos, medians):                            # fill the angles with measures
            ldata[p].dist = int(m*1000)   # to millimeters
        if ldata[0] == 0:
            ldata[0] = 200       #half robot width
        for i in range(1, len(ldata)):
            if ldata[i].dist == 0:
                ldata[i].dist = ldata[i-1].dist

        return ldata
    
    def update_joystick(self, datos):
        adv = 0.0
        rot = 0.0
        side = 0.0

        for x in datos.axes:
            if x.name == "advance":
                adv = x.value if np.abs(x.value) > 0.4 else 0
            if x.name == "rotate":
                rot = x.value if np.abs(x.value) > 0.1 else 0
            if x.name == "side":
                side = x.value if np.abs(x.value) > 0.4 else 0
        print("Joystick ", adv, rot, side)
        self.robot.set_base_angular_velocites([adv, side, rot])

    ##################################################################################
    # SUBSCRIPTION to sendData method from JoystickAdapter interface
    ###################################################################################
    def JoystickAdapter_sendData(self, data):
        self.joystick_newdata = [data, time.time()]

    ##################################################################################
    #                       Methods for CameraRGBDSimple
    # ===============================================================================
    #
    # getAll
    #
    def CameraRGBDSimple_getAll(self, camera):
        return RoboCompCameraRGBDSimple.TRGBD(self.cameras[camera]["rgb"], self.cameras[camera]["depth"])

    #
    # getDepth
    #
    def CameraRGBDSimple_getDepth(self, camera):
        return self.cameras[camera]["depth"]
    #
    # getImage
    #
    def CameraRGBDSimple_getImage(self, camera):
        return self.cameras[camera]["rgb"]

    #######################################################
    #### Laser
    #######################################################

    #
    # getLaserAndBStateData
    #
    def Laser_getLaserAndBStateData(self):
        bState = RoboCompGenericBase.TBaseState()
        return self.ldata, bState

    #
    # getLaserConfData
    #
    def Laser_getLaserConfData(self):
        ret = RoboCompLaser.LaserConfData()
        return ret

    #
    # getLaserData
    #
    def Laser_getLaserData(self):
        return self.ldata

    ##############################################
    ## Omnibase
    #############################################

    #
    # correctOdometer
    #
    def OmniRobot_correctOdometer(self, x, z, alpha):
        pass

    #
    # getBasePose
    #
    def OmniRobot_getBasePose(self):
        #
        # implementCODE
        #
        x = self.bState.x
        z = self.bState.z
        alpha = self.bState.alpha
        return [x, z, alpha]

    #
    # getBaseState
    #
    def OmniRobot_getBaseState(self):
        return self.bState

    #
    # resetOdometer
    #
    def OmniRobot_resetOdometer(self):
        pass

    #
    # setOdometer
    #
    def OmniRobot_setOdometer(self, state):
        pass

    #
    # setOdometerPose
    #
    def OmniRobot_setOdometerPose(self, x, z, alpha):
        pass

    #
    # setSpeedBase
    #
    def OmniRobot_setSpeedBase(self, advx, advz, rot):
        self.speed_robot = [advz, advx, rot]


    #
    # stopBase
    #
    def OmniRobot_stopBase(self):
        pass

    # ===================================================================
    # ===================================================================

   #self.hokuyo_base_front_left_semiangle = np.radians(self.hokuyo_base_front_left.get_perspective_angle()/2)
        #self.hokuyo_base_front_left_semiwidth = self.hokuyo_base_front_left.get_resolution()[0]/2
        #self.hokuyo_base_front_left_focal = self.hokuyo_base_front_left_semiwidth/np.tan(self.hokuyo_base_front_left_semiangle)
     
    # hokuyo_base_front_left_reading = self.hokuyo_base_front_left.capture_depth(in_meters=True)
                # hokuyo_base_front_right_reading = self.hokuyo_base_front_right.capture_depth(in_meters=True)
                # ldata = []
                # for i,d in enumerate(hokuyo_base_front_left_reading.T):
                #     angle = np.arctan2(i-(self.hokuyo_base_front_left_semiwidth), self.hokuyo_base_front_left_focal)
                #     dist = (d[0]/np.abs(np.cos(angle)))*1000
                #     ldata.append(RoboCompLaser.TData(angle-self.hokuyo_base_front_right_semiangle,dist))
                # for i,d in enumerate(hokuyo_base_front_right_reading.T):
                #     angle = np.arctan2(i-(self.hokuyo_base_front_right_semiwidth), self.hokuyo_base_front_right_focal)
                #     dist = (d[0]/np.abs(np.cos(angle)))*1000
                #     ldata.append(RoboCompLaser.TData(angle+self.hokuyo_base_front_right_semiangle,dist))
             
