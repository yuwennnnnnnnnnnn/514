# Museum Entrance Visitor Counter
### This project designed to count visitors entering and exiting a museum space, using sensors at the entrance and a physical display to present real-time occupancy information.
### Problem being solved：
Museums and galleries often rely on staff to manually count visitors at the entrance. Manual counting is inefficient and often inaccurate during peak hours.
This project automates visitor counting and shows occupancy through a physical gauge display.
### Proposed solution:
<img width="1150" height="354" alt="Screenshot 2026-01-19 at 6 31 43 PM" src="https://github.com/user-attachments/assets/3b4f9e9a-d327-4aee-a5b3-923e8c291a71" />
Sensing device: A device installed at the museum entrance that uses distance sensors to detect people entering and exiting. The device processes directional movement and wirelessly sends visitor count data.

Display device: physically displays the current visitor count or occupancy level, allowing staff to quickly understand space usage at a glance and adjust lighting and air conditioning accordingly.

<img width="1920" height="1080" alt="The Problem" src="https://github.com/user-attachments/assets/57ced770-f01d-4087-b668-7f51ab58e303" />

<img width="1920" height="1080" alt="The Problem-1" src="https://github.com/user-attachments/assets/c31ec462-12ca-431b-8428-f32c7ed7bab0" />
<img width="1920" height="1080" alt="The Problem" src="https://github.com/user-attachments/assets/9fb0e9ae-79c0-42c5-9bb8-9da70d53e984" />


### Device Components Table

| Device         | Component    | Part Number            | Function                  |
| -------------- | ------------ | ---------------------- | ------------------------- |
| Sensor Device  | IR Sensor x2 | Sharp GP2Y0A02YK0F     | Detects people enter/exit |
| Sensor Device  | MCU          | Xiao ESP32-c3          | Main controller           |
| Display Device | OLED Display | 0.96″ SSD1306          | Shows visitor count       |
| Display Device | LEDs ×4      | Kingbright WP7113      | Occupancy indicators      |
| Display Device | Button       | APEM 9008T10           | User control light        |
| Display Device | Battery      | Adafruit LiPo 1200 mAh | Power source              |
