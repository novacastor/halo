### 1. Download
1. Go to the **[Actions](../../actions)** tab at the top of this repository.
2. Click on the latest successful **Build AppImage** workflow run.
3. Scroll down to the **Artifacts** section and click **Halo-AppImage** to download the `.zip` file.
4. Extract the `.zip` file to get the `Halo-Linux-x86_64.AppImage` executable.

### 2. Make it Executable
Before you can run an AppImage, you need to give it permission to execute.

**Option A: Using the File Manager (GUI)**
* Right-click the `.AppImage` file and select **Properties**.
* Go to the **Permissions** tab.
* Check the box that says **"Allow executing file as program"** (or similar, depending on your desktop environment).

**Option B: Using the Terminal**
Open your terminal in the folder where you downloaded the file and run:
```bash
chmod +x Halo-Linux-x86_64.AppImage
```

### 3. Run It!
Simply double-click the Halo-Linux-x86_64.AppImage file in your file manager, or 
run it from the terminal:
```bash
./Halo-Linux-x86_64.AppImage
```
