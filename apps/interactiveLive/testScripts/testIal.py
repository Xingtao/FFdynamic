# -*- coding: utf-8 -*-

import time
import sys
import json
from requests import request
from google.protobuf.json_format import MessageToJson
sys.path.append("../build/protos")

import davStreamletSetting_pb2 as streamlet
import davWaveSetting_pb2 as wave
import ialRequest_pb2 as ial_request
import ialConfig_pb2 as ial_config

http_dst = "http://127.0.0.1:8080"

uri_create_room = "/api1/ial/create_room"
uri_add_new_input =  "/api1/ial/add_new_input_stream"
uri_add_new_output = "/api1/ial/add_new_output"
uri_close_one_input = "/api1/ial/close_one_input_stream"
uri_close_one_output = "/api1/ial/close_one_output"

uri_query_input_info = "/api1/ial/input_stream_info"
uri_query_output_info = "/api1/ial/output_stream_info"

uri_mute_unmute = "/api1/ial/mute_unmute_stream"
uri_mix_laytout_change = "/api1/ial/mix_layout_change"
uri_mix_backgroud_update = "/api1/ial/mix_backgroud_update"
uri_stop = "/api1/ial/stop"

uri_update_input_setting = "/api1/ial/update_input_setting"
uri_add_output_setting = "/api1/ial/add_output_setting"
uri_update_mix_setting = "/api1/ial/update_mix_setting"

out_setting_id_720p = "720p_2000kb"
out_setting_id_1080p = "1080p_4000kb"
out_setting_id_udp_720p = "udp_720p"

# chnage to your files
input1 = "bunny.avi"
input2 = "soccer_adidas_ad.mp4"
input3 = "nba_ad.mp4"

output_dir = "./" #"../testScripts"
fullurl_output_720 = "output_720p.flv"
fullurl_output_1080 = "output_1080p.flv"
fullurl_output_udp_720 = "udp://127.0.0.1:12345"

def get_new_output(output_setting_id, output_full_url):
    new_output = ial_request.AddNewOutputStream()
    new_output.output_setting_id = output_setting_id
    new_output.output_urls.append(output_full_url)
    return new_output

usage = '''
        1 -> create room
        2 -> add new input
        3 -> add new output
        4 -> close one input
        5 -> close one output
        6 -> mute/unmute
        7 -> layout change
        8 -> mix backgroud update
        10 -> ial stop

        x -> update input setting (not implemented)
        x -> update mix setting (not implemented)
        x -> add new output setting (not implemented)
        '''

def sendCreateRoom():
    create_room = ial_request.CreateRoom()
    create_room.room_id = "ial_test"
    create_room.input_urls.append(input1)
    #create_room.input_urls.append()
    create_room.room_output_base_url = output_dir
    new_output = create_room.output_stream_infos.add()
    new_output.output_setting_id = out_setting_id_720p
    new_output.output_urls.append(fullurl_output_720)

    response = request("POST", http_dst + uri_create_room, data = MessageToJson(create_room))
    print (response.url, response.text)

def addNewInput():
    new_input = ial_request.AddNewInputStream()
    choise = input("Enter choise 1, 2 or 3 to add input1, input2, input3: ")
    if choise == "1":
        new_input.input_url = input1
    elif choise == "2":
        new_input.input_url = input2
    elif choise == "3":
        new_input.input_url = input3
    else:
        print ("not valid choise " + choise + ", do nothing")
        return
    response = request("POST", http_dst + uri_add_new_input, data = MessageToJson(new_input))
    print (response.url, response.text)

def addNewOutput():
    new_output = ial_request.AddNewOutput()
    choise = input("Enter choise 1, for 1080p output setting; 2, for udp 720p setting: ")
    if choise == "1":
        new_output.output_setting_id = out_setting_id_1080p
    elif choise == "2":
        new_output.output_setting_id = out_setting_id_udp_720p
        new_output.output_urls.append(fullurl_output_udp_720)
    else:
        print ("not valid choise " + choise + ", do nothing")
        return

    response = request("POST", http_dst + uri_add_new_output, data = MessageToJson(new_output))
    print (response.url, response.text)

def closeOneInput():
    one_input = ial_request.CloseOneInputStream()
    choise = input("Enter choise 1, 2 or 3. to close input1, input2 or input3: ")
    if choise == "1":
        one_input.input_url = input1
    elif choise == "2":
        one_input.input_url = input2
    elif choise == "3":
        one_input.input_url = input3
    else:
        print ("not valid choise " + choise + ", do nothing")
        return
    response = request("POST", http_dst + uri_close_one_input, data = MessageToJson(one_input))
    print (response.url, response.text)

def closeOneOutput():
    one_output = ial_request.CloseOneOutput()
    one_output.output_setting_id = out_setting_id_1
    response = request("POST", http_dst + uri_close_one_output, data = MessageToJson(one_output))
    print (response.url, response.text)

def muteUnmute():
    choise = input("Enter choise 1, 2 to mute or unmute input1 & input2: ")
    audio_mute_unmute = ial_request.AudioMixMuteUnMute()
    if choise == "1":
        audio_mute_unmute.mute_input_urls.append(input1)
        audio_mute_unmute.mute_input_urls.append(input2)
    elif choise == "2":
        audio_mute_unmute.unmute_input_urls.append(input1)
        audio_mute_unmute.unmute_input_urls.append(input2)
    else:
        print ("not valid choise " + choise + ", do nothing")
        return
    response = request("POST", http_dst + uri_mute_unmute, data = MessageToJson(audio_mute_unmute))
    print (response.url, response.text)

def layoutChange():
    layout_change = ial_request.VideoMixChangeLayout()
    choise = input("Enter choise 1, 2 or 3. to choose: eSingle_1, eEqual_4, eEqual_9: ")
    if choise == "1":
        layout_change.new_layout.layout = wave.eSingle_1
    elif choise == "2":
        layout_change.new_layout.layout = wave.eEqual_4
    elif choise == "3":
        layout_change.new_layout.layout = wave.eEqual_9
    else:
        print ("not valid choise " + choise + ", do nothing")
        return
    # specifc coordinates not supported right now
    response = request("POST", http_dst + uri_mix_laytout_change, data = MessageToJson(layout_change))
    print (response.url, response.text)


def setMixNewBackgroud():
    updateBg = ial_request.VideoMixUpdateBackgroud()
    choise = input("Enter new backgroud image url:  ")
    if choise == "1":
        updateBg.backgroud_image_url = "../../../asset/ffdynamic-bg2.jpg"
    # specifc coordinates not supported right now
    updateBg.backgroud_image_url = choise
    response = request("POST", http_dst + uri_mix_backgroud_update, data = MessageToJson(updateBg))
    print (response.url, response.text)

def ialStop():
    response = request("POST", http_dst + uri_stop)
    print (response.url, response.text)

############################################################
if __name__ == "__main__":
    while True:
        try:
            print (usage)
            choise = input("Enter choise (or leave blank to finish): ")
            if choise == "":
                break
            elif choise == "1":
                sendCreateRoom()
            elif choise == "2":
                addNewInput()
            elif choise == "3":
                addNewOutput()
            elif choise == "4":
                closeOneInput()
            elif choise == "5":
                closeOneOutput()
            elif choise == "6":
                muteUnmute()
            elif choise == "7":
                layoutChange()
            elif choise == "8":
                setMixNewBackgroud()
            elif choise == "10":
                ialStop()
        except Exception as e:
            print (" choice of "+ str(choise) + " cause exception ", e)
    ##
    print ("=> exit")
