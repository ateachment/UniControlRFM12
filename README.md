# On-Demand Hot Water Circulation Control System (RFM12 / AVR)

This repository contains the robust, interrupt-driven firmware for an energy-efficient, on-demand Domestic Hot Water (DHW) circulation control system utilizing low-power **RFM12** ($433\text{ MHz}$) transceivers paired with **AVR** microcontrollers (**ATmega8-16PU** and **ATtiny2313**).

---

## 🛑 Problem Statement: Inefficient Hot Water Circulation

Continuous loop or simple timer-blind domestic hot water (DHW) circulation pumps are highly energy-inefficient. They constantly cycle hot water through building piping networks, causing severe standby thermal losses through walls and floors. This constant cycling results in high energy waste and accelerates mechanical wear on the pump.

### The Basement Connectivity Challenge (Why 433 MHz?)
Modern smart-home appliances often rely on $2.4\text{ GHz}$ or $5\text{ GHz}$ Wi-Fi (WLAN). However, in residential applications, the circulation pump and its controller are typically located deep within a concrete basement or cellar. 
* **Wi-Fi Failure:** High-frequency Wi-Fi signals suffer massive attenuation when passing through thick reinforced concrete floors and subterranean structural barriers, leading to frequent dropouts and dead zones.
* **The 433 MHz Advantage:** This system utilizes sub-GHz **$433\text{ MHz}$ RF transmission**. The longer wavelength provides vastly superior penetration capabilities through heavy masonry, masonry walls, and reinforced concrete, ensuring a flawless, long-range connection from living spaces straight down to the basement pump room without requiring network infrastructure.

To solve this sustainably, an **on-demand wireless triggering system** is implemented. The circulation pump is activated only when a manual user request is broadcast, running for a strictly defined interval before automatically cutting off.

---

## 🏗 System Architecture

The hardware layout establishes a dedicated, infrastructure-independent RF link split between an on-demand transmitter node and a mains-tied actuator receiver node.

```
+---------------------------------------+         433 MHz RF         +---------------------------------------+
|             TRANSMITTER               |                            |              RECEIVER                 |
| (Battery-Powered / Pulsed Low-Power)  | ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~  |      (Mains-Powered / Actuator)       |
| ATtiny2313 + RFM12 + DIP Mode Switch  |    Concrete Penetration    | ATmega8-16PU + RFM12 + 230V~ Relay    |
+---------------------------------------+                            +---------------------------------------+
```

### A. Transmitter Node (Battery-Powered)
Located within the living space for user accessibility. It is optimized for ultra-low quiescent current draw to preserve battery lifetime.
* **Microcontroller:** Microchip/Atmel **ATtiny2313** managing inputs and low-power state transitions.
* **Power Supply:** $3\text{V}$ DC rail provided by two series-connected AAA $1.5\text{V}$ batteries.
* **Manual Trigger Button:** Wakes the MCU from sleep to broadcast an activation payload.
 
  
* **Transceiver:** **RFM12 module** operating at $433\text{ MHz}$ via an external $\lambda/4$ whip antenna (`Ant2`). Connected over SPI (Pins `17`, `18`, `19` on the ATtiny2313) along with the active-low slave select (`nSEL`) tied to Pin `20`.

### B. Receiver Node (Mains-Powered)
Permanently installed in the basement near the DHW circulation pump.
* **Microcontroller:** Microchip/Atmel **ATmega8-16PU** acting as the central actuator and safety guard.
* **Power Supply:** Fixed $5\text{V}$ DC mains power adapter supplying the MCU and peripherals, with a local $10\,\mu\text{F}$ stabilization capacitor (`C1`).
* **Transceiver & Hardware Interrupts:** An **RFM12 module** with an external antenna (`Ant1`). The module's hardware interrupt line (`nIRQ`) is wired directly to the ATmega8’s external interrupt `INT0` (Pin `4` / `PD2`) for real-time packet capturing.
* **Actuator Interface:** Connected to an isolated **Relay Module** via an output control line. When a valid data frame passes the local software checksum validation routine, the ATmega8 triggers the relay to close the isolated $230\text{V}\sim$ AC circuit, powering up the high-voltage circulation pump for the allotted runtime.

---

## 🛡 High-Noise & Reflection Handling (Subterranean Firmware Patch)

Industrial basements can create severe RF multipath reflections and interference. Under extreme noise, the RFM12 can flood the `INT0` line with garbage data, causing a critical vulnerability known as **CPU Starvation (Interrupt Storm)**. If unchecked, the MCU spends $100\%$ of its cycles servicing the ISR, preventing the main loop from executing the timer code required to turn off the pump relay.

To guarantee absolute system safety and prevent the pump from getting stuck in an infinite `ON` state, the receiver employs a hardware-level decoupling sequence upon checksum failure:

```c
if(sum == checksum(RF12_Data+2))    // Checksum matches
{
    PORTD |= (1 << LED1);           // Success blink
    
    if (op_mode == MODE_TOGGLE)
    {
        PORTD ^= (1 << LED2);       // Invert relay state via RF
    }
    else
    {
        PORTD &= ~(1 << LED2);      // Turn relay ON
        relay_timer = max_timer_interval;
    }
}
else  // Checksum mismatch -> Heavy RF noise/reflections detected in the basement
{
    // 1. Instantly disable INT0 to shield the core CPU from the interrupt storm
    cbi(GICR, INT0); 

    // 2. Shut down the RF receiver stage to clear internal hardware buffers
    rf12_trans(0x8208); // Receive off

    // 3. Visual error feedback (Double-flash sequence on LED1)
    PORTD |= (1 << LED1);   
    _delay_ms(40);
    PORTD &= ~(1 << LED1);  
    _delay_ms(180);
    PORTD |= (1 << LED1);        

    // 4. Manually clear any accumulated garbage interrupt flags in the hardware register.
    // On ATmega8, the flag is cleared by writing a logical '1' to it!
    sbi(GIFR, INTF0); // Clear the INT0 flag
    
    // Note: Manual re-enabling of INT0 via sbi(GICR, INT0) is omitted here, 
    // as the subsequent rf12_rxrestart() function automatically re-enables it cleanly.
}
rf12_rxrestart(); // Re-initializes RFM12 FIFO mode and cleanly re-arms the INT0 line
```

## 📡 Hardware & Antenna Specifications
* **Antenna Configuration:** Avoid using folded or closed loops inside metal or thick plastic enclosures, as they suffer massive tuning shifts and severe attenuation. For reliable basement propagation, use a straight, free-hanging **$17.3\text{ cm}$ $\lambda/4$ wire monopole**, ensuring a precise $50\text{--}\Omega$ impedance match directly at the RFM12 output stage.
