# Home Assistant Dashboard for Project Aura

This folder contains a ready-to-use YAML configuration to visualize your Aura data.
It uses only standard Home Assistant cards. No HACS or external dependencies required.

![Dashboard Preview](preview.png)
(Add a screenshot named preview.png in this folder.)

## How to Add
You can add this as a new view (tab) to your existing dashboard.

1. Open your Home Assistant Dashboard.
2. Click the Pencil icon (Edit Dashboard) in the top right.
3. Click the 3 dots menu (top right) -> Raw configuration editor.
4. Scroll to the place where you want to insert the code (or replace existing code if starting fresh).
5. Paste the contents of `dashboard.yaml`.
6. Click Save.

## Entity Configuration
Your entity IDs might differ depending on how your MQTT auto-discovery named them (for example,
`sensor.aura_sfa30_voc` vs `sensor.livingroom_voc`).

If you see "Entity not found" warnings:
1. Go to Settings -> Devices & Services -> Entities.
2. Search for "Aura" to see your actual entity IDs.
3. Open the dashboard code and use Find & Replace to swap the IDs with yours.
