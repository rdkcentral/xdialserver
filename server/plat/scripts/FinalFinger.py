#!/usr/bin/env python3
"""Send out a M-SEARCH request and listening for responses."""
import cv2
import mediapipe as mp
import subprocess
import os, re
import time
import asyncio
import socket
import ssdp

mp_drawing = mp.solutions.drawing_utils
mp_drawing_styles = mp.solutions.drawing_styles
mp_hands = mp.solutions.hands

ip = ""
cap = cv2.VideoCapture(0)

class MyProtocol(ssdp.SimpleServiceDiscoveryProtocol):
    """Protocol to handle responses and requests."""

    def response_received(self, response: ssdp.SSDPResponse, addr: tuple):
        """Handle an incoming response."""
        found = 0
        ipstr = ""
        for header in response.headers:
            if "USN" in header[0] and "dial" in header[1]:
               found = found + 1
            elif "Location" in header[0]:
               ipstr = header[1]
               found = found + 1

        if(found == 2 ):
            global ip
            spl_char = ":"
            res = ipstr.rpartition(spl_char)[0]
            ip = res.lstrip("https://")


loop = asyncio.get_event_loop()
connect = loop.create_datagram_endpoint(MyProtocol, family=socket.AF_INET)
transport, protocol = loop.run_until_complete(connect)

# Send out an M-SEARCH request, requesting all service types.
search_request = ssdp.SSDPRequest(
        "M-SEARCH",
        headers={
            "HOST": "239.255.255.250:1900",
            "MAN": '"ssdp:discover"',
            "MX": "4",
            "ST": "ssdp:all",
        },
    )
search_request.sendto(transport, (MyProtocol.MULTICAST_ADDRESS, 1900))
# Keep on running for 4 seconds.
try:
    loop.run_until_complete(asyncio.sleep(4))
except KeyboardInterrupt:
    pass
if(ip == ""):
    print("No Valid DIAL Server found so Exiting the program")
    raise SystemExit
else:
    print("DIAL host IP:",ip)

end_time = time.time()
with mp_hands.Hands(model_complexity=0,min_detection_confidence=0.5,min_tracking_confidence=0.5) as hands:
  while cap.isOpened():
    success, image = cap.read()
    image = cv2.flip(image, 1)
    if not success:
      print("Ignoring empty camera frame.")
      break

    # To improve performance, optionally mark the image as not writeable to
    # pass by reference.
    image.flags.writeable = False
    image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    results = hands.process(image)

    # Draw the hand annotations on the image.
    image.flags.writeable = True
    image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)

    # Initially set finger count to 0 for each cap
    fingerCount = 0

    if results.multi_hand_landmarks:

      for hand_landmarks in results.multi_hand_landmarks:
        # Get hand index to check label (left or right)
        handIndex = results.multi_hand_landmarks.index(hand_landmarks)
        handLabel = results.multi_handedness[handIndex].classification[0].label

        # Set variable to keep landmarks positions (x and y)
        handLandmarks = []

        # Fill list with x and y positions of each landmark
        for landmarks in hand_landmarks.landmark:
          handLandmarks.append([landmarks.x, landmarks.y])

        # Test conditions for each finger: Count is increased if finger is 
        #   considered raised.
        # Thumb: TIP x position must be greater or lower than IP x position, 
        #   deppeding on hand label.
        if handLabel == "Left" and handLandmarks[4][0] > handLandmarks[3][0]:
          fingerCount = fingerCount+1
        elif handLabel == "Right" and handLandmarks[4][0] < handLandmarks[3][0]:
          fingerCount = fingerCount+1

        # Other fingers: TIP y position must be lower than PIP y position, 
        #   as image origin is in the upper left corner.
        if handLandmarks[8][1] < handLandmarks[6][1]:       #Index finger
          fingerCount = fingerCount+1
        if handLandmarks[12][1] < handLandmarks[10][1]:     #Middle finger
          fingerCount = fingerCount+1
        if handLandmarks[16][1] < handLandmarks[14][1]:     #Ring finger
          fingerCount = fingerCount+1
        if handLandmarks[20][1] < handLandmarks[18][1]:     #Pinky
          fingerCount = fingerCount+1

        # Draw hand landmarks 
        mp_drawing.draw_landmarks(
            image,
            hand_landmarks,
            mp_hands.HAND_CONNECTIONS,
            mp_drawing_styles.get_default_hand_landmarks_style(),
            mp_drawing_styles.get_default_hand_connections_style())

    # Passing the curl command to the device
    flag  = 0
    if(time.time() - end_time > 3):
        end_time = time.time()
        flag = 1

    if(flag == 1):
        # Display finger count
        cv2.putText(image, str(fingerCount), (50, 450), cv2.FONT_HERSHEY_SIMPLEX, 3, (255, 0, 0), 10)
    
    if (fingerCount == 1 and flag == 1):
        subprocess.run('echo "Action -> UP "', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=up'
                    ])
    elif (fingerCount == 2 and flag == 1):
        subprocess.run('echo "Action -> DOWN"', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=down'
                    ])
    elif (fingerCount == 3 and flag == 1):
        subprocess.run('echo "Action-> LEFT"', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=left'
                    ])
    elif (fingerCount == 4 and flag == 1):
        subprocess.run('echo "Action-> RIGHT"', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=right'
                    ])
    elif (fingerCount == 5 and flag == 1):
        subprocess.run('echo "Action-> PLAY/PAUSE"', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=play'
                    ])
    elif (fingerCount == 6 and flag == 1):
        subprocess.run('echo "Action-> SELECT"', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=select'
                    ])
    elif (fingerCount == 7 and flag == 1):
        subprocess.run('echo "Action-> EXIT"', shell=True)
        subprocess.Popen([
                        'curl', 
						'-X',
                        'POST', 
						'http://' + ip + ':56889/apps/system?action=exit'
                    ])
    # Display image
    cv2.imshow('MediaPipe Hands', image)
    if cv2.waitKey(5) & 0xFF == 27:
      break
cap.release()
