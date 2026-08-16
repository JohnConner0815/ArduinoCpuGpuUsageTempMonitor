using System.Drawing;
using System.Windows.Forms;

namespace ArduinoCpuGpuUsageTempMonitor
{
    public class DarkGreenProgressBar : ProgressBar
    {
        public DarkGreenProgressBar()
        {
            this.SetStyle(ControlStyles.UserPaint, true);
            this.ForeColor = Color.FromArgb(0, 140, 0); // Darker, professional green
            this.BackColor = Color.FromArgb(30, 30, 30); // Dark background for the bar
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Rectangle rec = e.ClipRectangle;
            if (ProgressBarRenderer.IsSupported)
                ProgressBarRenderer.DrawHorizontalBar(e.Graphics, rec);

            rec.Inflate(-3, -3); // Inner padding
            rec.Width = (int)(rec.Width * ((double)Value / Maximum));

            if (rec.Width > 0)
            {
                using (SolidBrush brush = new SolidBrush(this.ForeColor))
                {
                    e.Graphics.FillRectangle(brush, rec);
                }
            }
        }
    }
}