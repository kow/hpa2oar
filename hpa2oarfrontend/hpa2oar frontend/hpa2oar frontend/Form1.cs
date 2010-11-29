using System;
using System.IO;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace hpa2oar_frontend
{
    public partial class Form1 : Form
    {
        string Chosen_File = "";
        string Chosen_RAW = "";
        string Saved_File = "";

        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            openFileDialog1.Title = "Select HPA File";
            openFileDialog1.InitialDirectory = "E:";
            openFileDialog1.FileName = "";
            openFileDialog1.Filter = "HPA|*.hpa|All Files|*.*";
            openFileDialog1.ShowDialog();
            Chosen_File = openFileDialog1.FileName;
            label1.Text = Chosen_File;
        }

        private void button2_Click(object sender, EventArgs e)
        {
            saveFileDialog1.InitialDirectory = "E:";
            saveFileDialog1.Title = "Choose OAR Destination";
            saveFileDialog1.FileName = "untitled.oar";
            saveFileDialog1.Filter = "OAR|*.oar";
            if (saveFileDialog1.ShowDialog() != DialogResult.Cancel)
            {
                Saved_File = saveFileDialog1.FileName;
                string path = Path.GetDirectoryName(Saved_File);
                label2.Text = path;

                Process proc = new Process();
                proc.StartInfo.FileName = @"hpa2oar.exe";
                proc.StartInfo.Arguments = "--hpa \"" + Chosen_File + "\" --oar \"" + path
                    + "\\TEMP\" --terrain \"" + Chosen_RAW + "\"";
                proc.Start();
                proc.WaitForExit();

                string arg = "a -ttar \"" + path + "\\TEMP\\temp.tar\" \"" + path + "\\TEMP\\*\"";

                Process tar = new Process();
                tar.StartInfo.FileName = @"7za.exe";
                tar.StartInfo.Arguments = arg;
                tar.Start();
                tar.WaitForExit();

                arg = "a -tgzip \"" + Saved_File + "\" \"" + path + "\\TEMP\\temp.tar";

                Process gzip = new Process();
                gzip.StartInfo.FileName = @"7za.exe";
                gzip.StartInfo.Arguments = arg;
                gzip.Start();
            }
        }

        private void button3_Click(object sender, EventArgs e)
        {
            openFileDialog1.Title = "Select RAW File";
            string path = Path.GetDirectoryName(Chosen_File);
            openFileDialog1.InitialDirectory = path;
            openFileDialog1.FileName = "";
            openFileDialog1.Filter = "RAW|*.raw|All Files|*.*";
            openFileDialog1.ShowDialog();
            Chosen_RAW = openFileDialog1.FileName;
            label3.Text = Chosen_RAW;
        }
    }
}
