# RFID Smart Card Management System

## Overview

This project is developed using Arduino UNO and the RC522 RFID Reader Module. The main purpose of the system is to identify users through RFID cards and manage their balance information.

The system consists of one Master Card and three Slave Cards. The Master Card acts as an administrator card, while the Slave Cards are assigned to users. When a card is scanned, the system verifies the card and displays the user's details along with balance information.

This project can be used in schools, colleges, gaming zones, shopping malls, offices, libraries, and other places where smart card authentication is required.

## Features

* RFID-based user authentication
* One Master Card and three Slave Cards
* User information display
* Balance checking
* Credit and debit management
* Fast card verification
* Simple and low-cost implementation

## Hardware Used

* Arduino UNO
* RC522 RFID Reader
* 1 Master RFID Card
* 3 Slave RFID Cards
* Breadboard
* Jumper Wires
* USB Cable

## Software Used

* Embedded C Programming
* Arduino IDE
* Serial Communication
* RFID Card UID Processing
* Basic Input/Output Operations

## How It Works

1. The Master Card is used for administrative access.
2. The Slave Cards are assigned to users.
3. When a card is scanned, the RFID reader reads the card UID.
4. The Arduino verifies the card.
5. User details and balance information are displayed.
6. Credit or debit operations can be performed based on the application.

## Sample Output
Master card scan
Name : Manju

ID Number : 22ECE103

Balance : ₹15000

Status : Master Card Verified Successfully

slave card

Name : Kiruba Kanth

ID Number : 22ECE101

Balance : ₹1500

Credit : ₹500

Debit : ₹200

Status : Verified Successfully

## Hardware Setup

<img width="1600" height="980" alt="image" src="https://github.com/user-attachments/assets/643ecdce-941b-433e-9e88-9774513c2bb0" />


## Output

<img width="702" height="1600" alt="WhatsApp Image 2026-06-05 at 4 49 16 PM" src="https://github.com/user-attachments/assets/2c4b4810-5fc4-4a8f-bc70-7b0aa54c7d9d" />


## Demo Video

https://drive.google.com/file/d/1rTuxGEcDlZsFOULA0yZBo-gYmupdVAiU/view?usp=drivesdk

## Applications

* Student ID Management
* Library Systems
* Gaming Zone Membership Cards
* Shopping Mall Loyalty Cards
* Office Access Control
* Hostel Management
* Smart Wallet Systems
* Attendance Monitoring

## Advantages

* Easy to use
* Low cost
* Secure authentication
* Fast response
* Reliable performance

## Future Improvements

* Cloud database integration
* Mobile application support
* Online balance management
* IoT connectivity
* Web dashboard monitoring

## Project Structure

RFID-Smart-Card-Management-System/

├── README.md

├── RFID_Smart_Card_System.ino

├── Circuit_Diagram/

├── Images/

├── Documentation/

└── Demo/
