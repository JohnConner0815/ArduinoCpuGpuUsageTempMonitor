using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;
using LibreHardwareMonitor.Hardware;

namespace ArduinoCpuGpuUsageTempMonitor
{
    public class MainForm : Form
    {
        [System.Runtime.InteropServices.DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr hWnd);

        private SerialPort serialPort;
        private System.Windows.Forms.Timer pollTimer;

        // Usage Monitors (Performance Counters)
        private PerformanceCounter cpuCounter = new PerformanceCounter("Processor", "% Processor Time", "_Total");
        private List<PerformanceCounter> gpuCounters = new List<PerformanceCounter>();
        private DateTime lastCounterRefresh = DateTime.MinValue;
        private int lastValidCpu = 0;
        private int lastValidGpu = 0;

        // Thread-safety lock for GPU counters
        private readonly object _gpuLock = new object();

        // Temperature Monitors (LibreHardwareMonitor)
        private Computer computer;
        private UpdateVisitor updateVisitor = new UpdateVisitor();

        // UI Controls - Top Row
        private ComboBox cmbComPorts;
        private Button btnRefresh;
        private ComboBox cmbPollingRate;
        private Button btnConnect;
        private Button btnDisconnect;

        // UI Controls - Data Display
        private Label lblCpuUsageText;
        private Label lblCpuTempText;
        private DarkGreenProgressBar progressBarCpu;

        private Label lblGpuUsageText;
        private Label lblGpuTempText;
        private DarkGreenProgressBar progressBarGpu;

        // Tray Icon
        private NotifyIcon trayIcon;
        private ContextMenuStrip trayMenu;
        private Icon iconGreen;
        private Icon iconRed;

        public MainForm()
        {
            InitializeGpuCounters();
            InitializeHardwareMonitor();
            InitializeUI();
            LoadComPorts();
            SetupTrayIcon();

            // Start updating UI immediately on launch
            UpdateTimerInterval();
            pollTimer.Start();
        }

        #region Initialization
        private void InitializeGpuCounters()
        {
            if (!PerformanceCounterCategory.Exists("GPU Engine"))
            {
                MessageBox.Show("GPU Performance Counters not found. Requires Windows 10+.", "OS Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            RefreshGpuCounters();
        }

        private void RefreshGpuCounters()
        {
            lock (_gpuLock)
            {
                try
                {
                    var category = new PerformanceCounterCategory("GPU Engine");
                    var currentInstanceNames = new HashSet<string>(category.GetInstanceNames().Where(i => i.EndsWith("engtype_3D")));

                    for (int i = gpuCounters.Count - 1; i >= 0; i--)
                    {
                        if (!currentInstanceNames.Contains(gpuCounters[i].InstanceName))
                        {
                            gpuCounters[i].Dispose();
                            gpuCounters.RemoveAt(i);
                        }
                    }

                    var existingNames = new HashSet<string>(gpuCounters.Select(c => c.InstanceName));
                    foreach (string name in currentInstanceNames)
                    {
                        if (!existingNames.Contains(name))
                        {
                            gpuCounters.Add(new PerformanceCounter("GPU Engine", "Utilization Percentage", name, true));
                        }
                    }
                    lastCounterRefresh = DateTime.Now;
                }
                catch { }
            }
        }

        private void InitializeHardwareMonitor()
        {
            computer = new Computer
            {
                IsCpuEnabled = true,
                IsGpuEnabled = true,
                IsMemoryEnabled = false,
                IsStorageEnabled = false,
                IsMotherboardEnabled = false,
                IsNetworkEnabled = false,
                IsControllerEnabled = false
            };
            computer.Open();
            computer.Accept(updateVisitor);
        }

        private void InitializeUI()
        {
            this.Text = "ArduinoCpuGpuUsageTempMonitor";
            this.ClientSize = new Size(500, 230);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.MinimizeBox = true;

            int y = 20;
            int dropdownWidth = 200;
            int buttonWidth = 80;
            int spacing = 10;
            int x = 15;

            // Row 1: COM Port & Refresh
            cmbComPorts = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = dropdownWidth, Location = new Point(x, y) };
            btnRefresh = new Button { Text = "Refresh", Width = buttonWidth, Location = new Point(x + dropdownWidth + spacing, y) };
            btnRefresh.Click += (s, e) => LoadComPorts();

            // Row 2: Polling Rate, Connect, Disconnect
            y += 35;
            cmbPollingRate = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = dropdownWidth, Location = new Point(x, y) };
            cmbPollingRate.Items.AddRange(new object[] { "1.0 s", "0.75 s", "0.5 s", "0.25 s", "0.1 s" });
            cmbPollingRate.SelectedIndex = 2; // Default 0.5s

            btnConnect = new Button { Text = "Connect", Width = buttonWidth, Location = new Point(x + dropdownWidth + spacing, y) };
            btnDisconnect = new Button { Text = "Disconnect", Width = buttonWidth + 10, Location = new Point(x + dropdownWidth + spacing + buttonWidth + spacing, y), Enabled = false };

            btnConnect.Click += BtnConnect_Click;
            btnDisconnect.Click += BtnDisconnect_Click;

            // Row 3: CPU Text & Temp Text
            y += 45;
            lblCpuUsageText = new Label { Text = "CPU Usage:", Location = new Point(x, y), AutoSize = true };
            lblCpuTempText = new Label { Text = "0 °C", Location = new Point(this.ClientSize.Width - 100, y), AutoSize = true, Font = new Font(Font.FontFamily, Font.Size, FontStyle.Bold) };

            // Row 4: CPU Progress Bar
            y += 22;
            progressBarCpu = new DarkGreenProgressBar { Width = this.ClientSize.Width - 30, Location = new Point(x, y), Height = 25 };

            // Row 5: GPU Text & Temp Text
            y += 40;
            lblGpuUsageText = new Label { Text = "GPU Usage:", Location = new Point(x, y), AutoSize = true };
            lblGpuTempText = new Label { Text = "0 °C", Location = new Point(this.ClientSize.Width - 100, y), AutoSize = true, Font = new Font(Font.FontFamily, Font.Size, FontStyle.Bold) };

            // Row 6: GPU Progress Bar
            y += 22;
            progressBarGpu = new DarkGreenProgressBar { Width = this.ClientSize.Width - 30, Location = new Point(x, y), Height = 25 };

            // Add controls to form
            this.Controls.AddRange(new Control[] {
                cmbComPorts, btnRefresh, cmbPollingRate, btnConnect, btnDisconnect,
                lblCpuUsageText, lblCpuTempText, progressBarCpu,
                lblGpuUsageText, lblGpuTempText, progressBarGpu
            });

            pollTimer = new System.Windows.Forms.Timer();
            pollTimer.Tick += PollTimer_Tick;
        }
        #endregion

        #region UI & Connection Events
        protected override void WndProc(ref Message m)
        {
            const int WM_SYSCOMMAND = 0x0112;
            const int SC_MINIMIZE = 0xF020;
            if (m.Msg == WM_SYSCOMMAND && m.WParam.ToInt32() == SC_MINIMIZE)
            {
                this.Hide();
                m.Result = IntPtr.Zero;
                return;
            }
            base.WndProc(ref m);
        }

        private void LoadComPorts()
        {
            string selectedPort = cmbComPorts.SelectedItem?.ToString();
            cmbComPorts.Items.Clear();
            cmbComPorts.Items.AddRange(SerialPort.GetPortNames());
            if (cmbComPorts.Items.Count == 0) cmbComPorts.Items.Add("No COM ports found");
            if (selectedPort != null && cmbComPorts.Items.Contains(selectedPort))
                cmbComPorts.SelectedItem = selectedPort;
            else
                cmbComPorts.SelectedIndex = 0;
        }

        private void SetupTrayIcon()
        {
            iconGreen = CreateSolidIcon(Color.Green);
            iconRed = CreateSolidIcon(Color.Red);
            trayMenu = new ContextMenuStrip();
            trayMenu.Items.Add("Show", null, (s, e) => { ShowWindow(); });
            trayMenu.Items.Add(new ToolStripSeparator());
            trayMenu.Items.Add("Close", null, (s, e) => { Application.Exit(); });
            trayIcon = new NotifyIcon { Icon = iconRed, ContextMenuStrip = trayMenu, Visible = true, Text = "Monitor - Disconnected" };
            trayIcon.DoubleClick += (s, e) => { ShowWindow(); };
        }

        private void ShowWindow()
        {
            if (this.InvokeRequired) { this.BeginInvoke((MethodInvoker)delegate { ShowWindow(); }); return; }
            this.Show();
            SetForegroundWindow(this.Handle);
            this.Activate();
        }

        private Icon CreateSolidIcon(Color color)
        {
            Bitmap bmp = new Bitmap(16, 16);
            using (Graphics g = Graphics.FromImage(bmp)) { g.Clear(color); }
            return Icon.FromHandle(bmp.GetHicon());
        }

        private void BtnConnect_Click(object sender, EventArgs e)
        {
            if (cmbComPorts.SelectedItem == null || cmbComPorts.SelectedItem.ToString().StartsWith("No")) return;
            try
            {
                serialPort = new SerialPort(cmbComPorts.SelectedItem.ToString(), 19200);
                serialPort.Open();
                btnConnect.Enabled = false;
                btnDisconnect.Enabled = true;
                cmbComPorts.Enabled = false;
                btnRefresh.Enabled = false;
                trayIcon.Icon = iconGreen;
                trayIcon.Text = "Monitor - Connected";
            }
            catch
            {
                MessageBox.Show("Could not open COM Port.", "Connection Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                SilentDisconnect();
            }
        }

        private void BtnDisconnect_Click(object sender, EventArgs e) { SilentDisconnect(); }

        private void SilentDisconnect()
        {
            if (serialPort != null && serialPort.IsOpen) { try { serialPort.Close(); } catch { } }
            serialPort = null;
            btnConnect.Enabled = true;
            btnDisconnect.Enabled = false;
            cmbComPorts.Enabled = true;
            btnRefresh.Enabled = true;
            progressBarCpu.Value = 0;
            progressBarGpu.Value = 0;
            lblCpuTempText.Text = "0 °C";
            lblGpuTempText.Text = "0 °C";
            trayIcon.Icon = iconRed;
            trayIcon.Text = "Monitor - Disconnected";
        }

        private void UpdateTimerInterval()
        {
            string selected = cmbPollingRate.SelectedItem.ToString();
            int ms = 500;
            if (selected.Contains("1.0")) ms = 1000;
            else if (selected.Contains("0.75")) ms = 750;
            else if (selected.Contains("0.5")) ms = 500;
            else if (selected.Contains("0.25")) ms = 250;
            else if (selected.Contains("0.1")) ms = 100;
            pollTimer.Interval = ms;
        }
        #endregion

        #region Data Gathering & Sending
        private async void PollTimer_Tick(object sender, EventArgs e)
        {
            UpdateTimerInterval();

            int cpuLoad = 0;
            int gpuLoad = 0;
            float cpuTemp = 0;
            float gpuTemp = 0;

            // Offload ALL data reading to a background thread to prevent UI stuttering
            await Task.Run(() =>
            {
                // 1. Get Usage (with glitch filter)
                int rawCpu = (int)Math.Max(0, Math.Min(100, cpuCounter.NextValue()));
                int rawGpu = Math.Max(0, Math.Min(100, GetGpuLoad()));

                if (rawCpu == 0 && lastValidCpu > 15) { rawCpu = lastValidCpu; } else { lastValidCpu = rawCpu; }
                if (rawGpu == 0 && lastValidGpu > 15) { rawGpu = lastValidGpu; } else { lastValidGpu = rawGpu; }

                cpuLoad = rawCpu;
                gpuLoad = rawGpu;

                // 2. Get Temperatures (unfiltered)
                computer.Accept(updateVisitor);
                cpuTemp = GetCpuTemperature();
                gpuTemp = GetGpuHotSpotTemperature();
            });

            // 3. Update UI safely on the main thread (this is nearly instantaneous)
            try
            {
                progressBarCpu.Value = cpuLoad;
                progressBarGpu.Value = gpuLoad;
                lblCpuTempText.Text = $"{cpuTemp:F0} °C";
                lblGpuTempText.Text = $"{gpuTemp:F0} °C";
            }
            catch { }

            // 4. Send Data over Serial (only if connected)
            try
            {
                if (serialPort != null && serialPort.IsOpen)
                {
                    serialPort.WriteLine($"{cpuLoad},{gpuLoad},{cpuTemp:F0},{gpuTemp:F0}");
                }
            }
            catch
            {
                SilentDisconnect();
            }
        }

        private int GetGpuLoad()
        {
            lock (_gpuLock)
            {
                if ((DateTime.Now - lastCounterRefresh).TotalSeconds > 2 || gpuCounters.Count == 0) { RefreshGpuCounters(); }
                float totalLoad = 0;
                bool needsRefresh = false;
                foreach (var counter in gpuCounters)
                {
                    try { totalLoad += counter.NextValue(); }
                    catch { needsRefresh = true; }
                }
                if (needsRefresh) { RefreshGpuCounters(); }
                return (int)Math.Min(100, Math.Max(0, totalLoad));
            }
        }

        private float GetCpuTemperature()
        {
            try
            {
                var cpu = computer.Hardware.FirstOrDefault(h => h.HardwareType == HardwareType.Cpu);
                if (cpu != null)
                {
                    var temps = cpu.Sensors.Where(s => s.SensorType == SensorType.Temperature).ToList();
                    if (temps.Any())
                    {
                        var packageTemp = temps.FirstOrDefault(s => s.Name.Contains("Package") || s.Name.Contains("Tdie") || s.Name.Contains("Core (Tctl/Tdie)"));
                        if (packageTemp != null && packageTemp.Value.HasValue) return packageTemp.Value.Value;
                        return temps.Max(s => s.Value ?? 0f);
                    }
                }
            }
            catch { }
            return 0f;
        }

        private float GetGpuHotSpotTemperature()
        {
            try
            {
                var gpu = computer.Hardware.FirstOrDefault(h => h.HardwareType == HardwareType.GpuAmd || h.HardwareType == HardwareType.GpuNvidia);
                if (gpu != null)
                {
                    var temps = gpu.Sensors.Where(s => s.SensorType == SensorType.Temperature).ToList();
                    if (temps.Any())
                    {
                        var hotSpotTemp = temps.FirstOrDefault(s => s.Name.Contains("Hot Spot") || s.Name.Contains("Junction") || s.Name.Contains("GPU Hotspot"));
                        if (hotSpotTemp != null && hotSpotTemp.Value.HasValue) return hotSpotTemp.Value.Value;
                        return temps.Max(s => s.Value ?? 0f);
                    }
                }
            }
            catch { }
            return 0f;
        }
        #endregion

        #region Cleanup
        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (e.CloseReason == CloseReason.UserClosing) { e.Cancel = true; this.Hide(); return; }

            pollTimer?.Stop();
            if (serialPort != null && serialPort.IsOpen) serialPort.Close();

            // Dispose usage counters
            cpuCounter.Dispose();
            foreach (var counter in gpuCounters) { counter.Dispose(); }

            // Dispose temp monitor
            computer?.Close();

            trayIcon?.Dispose();
            iconGreen?.Dispose();
            iconRed?.Dispose();
            base.OnFormClosing(e);
        }
        #endregion
    }

    // Required by LibreHardwareMonitor to update sensor values
    internal class UpdateVisitor : IVisitor
    {
        public void VisitComputer(IComputer computer) { computer.Traverse(this); }
        public void VisitHardware(IHardware hardware) { hardware.Update(); foreach (IHardware subHardware in hardware.SubHardware) subHardware.Accept(this); }
        public void VisitSensor(ISensor sensor) { }
        public void VisitParameter(IParameter parameter) { }
    }
}